/****************************************************************
**midi.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-04-20.
*
* Description: Interface for playing midi files.
*
*****************************************************************/
#include "midi.hpp"

// Revolution Now
#include "aliases.hpp"
#include "enum.hpp"
#include "errors.hpp"
#include "fmt-helper.hpp"
#include "init.hpp"
#include "logging.hpp"
#include "rand.hpp"
#include "time.hpp"

// base-util
#include "base-util/non-copyable.hpp"

// midifile (FIXME)
#include "../extern/midifile/include/MidiFile.h"

// rtmidi
#include "RtMidi.h"

// Abseil
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"

// C++ standard library
#include <queue>
#include <set>
#include <thread>
#include <vector>

using namespace std;

#define RTMIDI_WARN( exception_object ) \
  logger->warn( "rtmidi warning: {}",   \
                exception_object.getMessage() )

namespace rn {

namespace {

/****************************************************************
** Midi I/O
*****************************************************************/
// This class is a wrapper around the RtMidiOut object and han-
// dles creating and initializing it. Initialization consists of
// finding an appropriate midi output port and opening that port.
//
// This object can only be created with the static `create`
// method. If said method succeeds to return a MidiIO object then
// that means that MIDI music can and should be playable.
class MidiIO : public util::movable_only {
public:
  MidiIO( MidiIO&& rhs ) {
    out = std::move( rhs.out );
    rhs.release();
  }

  void close() {
    if( out ) out->closePort();
  }

  void release() { out = nullptr; }

  ~MidiIO() { close(); }

  static Opt<MidiIO> create() {
    MidiIO res;
    try {
      res.out.reset( new RtMidiOut() );
    } catch( RtMidiError const& error ) {
      RTMIDI_WARN( error );
      return nullopt;
    }
    auto maybe_port = res.find_midi_output_port();
    if( !maybe_port ) return nullopt;
    // This may be called from the midi thread.
    auto callback = []( RtMidiError::Type,
                        const std::string& error_text, void* ) {
      logger->warn( "rt-midi: {}", error_text );
    };
    res.out->setErrorCallback( callback, /*userdata=*/nullptr );

    try {
      res.out->openPort( *maybe_port );
    } catch( RtMidiError const& error ) {
      RTMIDI_WARN( error );
      logger->warn( "failed to open MIDI output port {}." );
      return nullopt;
    }
    logger->info( "using MIDI output port #{}", *maybe_port );
    return res;
  }

  void send_midi_message( smf::MidiEvent const& event ) {
    size_t const max_event_size = 200;
    // Should be more than large enough for any event. The
    // sendMessage API wants a vector, so we just use a static
    // one so that we don't have to do a heap allocation on each
    // event.
    static vector<unsigned char> bytes = [&]() {
      vector<unsigned char> res;
      res.resize( max_event_size );
      return res;
    }();
    if( event.size() > max_event_size ) {
      // Should be extremely rare.
      logger->warn(
          "MIDI event size {} is larger than max (={})",
          event.size(), max_event_size );
      return;
    }
    for( size_t i = 0; i < event.size(); ++i )
      bytes[i] = event[i];
    try {
      // Do not use the vector overload here because our vector
      // is always a fixed size.
      out->sendMessage( &bytes[0], event.size() );
    } catch( RtMidiError const& error ) { RTMIDI_WARN( error ); }
  }

  // Apparently there is a midi message called "all notes off",
  // but it is not clear that it works in the same way on all
  // devices. So just to be sure, we implement this by sending a
  // separate "Note Off" message for every note on every channel.
  void all_notes_off() {
    vector<uint8_t> message;
    message.resize( 3 );

    for( uint8_t channel = 0; channel < 16; ++channel ) {
      for( uint8_t note = 0; note < 128; ++note ) {
        // Note Off:
        //   1000nnnn : nnnn    = channel
        //   0kkkkkkk : kkkkkkk = note
        //   0vvvvvvv : vvvvvvv = velocity
        message[0] = 128 + channel;
        message[1] = 0 + note;
        message[2] = 0 + 127;
        try {
          out->sendMessage( &message );
        } catch( RtMidiError const& error ) {
          RTMIDI_WARN( error );
        }
      }
    }
  }

private:
  MidiIO() = default;

  vector<pair<int, string>> scan_midi_output_ports() {
    vector<pair<int, string>> res;
    try {
      auto num_ports = int( out->getPortCount() );
      for( auto i = 0; i < num_ports; i++ )
        res.push_back( {i, out->getPortName( i )} );
    } catch( RtMidiError& error ) { RTMIDI_WARN( error ); }
    return res;
  }

  bool is_valid_output_port_name( string_view port_name ) {
    return absl::StrContains( absl::AsciiStrToLower( port_name ),
                              "fluid" );
    // TODO: figure out why timidity doesn't seem to work.
    //|| absl::StrContains( absl::AsciiStrToLower( port_name ),
    //                        "timidity" );
  }

  Opt<int> find_midi_output_port() {
    auto ports = scan_midi_output_ports();
    logger->info( "found {} midi output ports.", ports.size() );
    Opt<int> res;
    for( auto const& [port, name] : ports ) {
      logger->info( "  MIDI output port #{}: {}", port, name );
      if( !res && is_valid_output_port_name( name ) ) res = port;
    }
    if( !res ) {
      if( ports.size() == 0 )
        logger->warn( "no midi output ports available." );
      else
        logger->warn(
            "failed to find recognizable midi output port." );
      return nullopt;
    }
    return res;
  }

  // Apart from initialization and cleanup (by the main thread)
  // this should only be used by the midi thread since it is un-
  // known whether it is thread safe.
  unique_ptr<RtMidiOut> out{};
};

// This will be nullopt if midi music cannot be played for any
// reason.
Opt<MidiIO> g_midi;

/****************************************************************
** Midi Thread
*****************************************************************/
// Will be left as nullopt if the midi subsystem fails to ini-
// tialize. Music errors are generally not fatal for the game.
Opt<thread> g_midi_thread;

// Class used for communicating with the midi thread in a thread
// safe way.
class MidiCommunication {
public:
  MidiCommunication() = default;

  // This can be called from the midi thread or main thread.
  e_midi_player_state state() {
    lock_guard<mutex> lock( mutex_ );
    return state_;
  }

  // This should only be called from the midi thread itself.
  void set_state( e_midi_player_state new_state ) {
    lock_guard<mutex> lock( mutex_ );
    state_ = new_state;
  }

  // Intended to be called by the main thread.
  void set_playlist( set<string> const& files ) {
    lock_guard<mutex> lock( mutex_ );
    playlist_.clear();
    for( auto const& f : files ) playlist_.push_back( f );
  }

  // Intended to be called by the midi thread.
  vector<string> playlist() {
    lock_guard<mutex> lock( mutex_ );
    return playlist_; // copy
  }

  // Return copy for thread safety (we don't want the caller to
  // have a reference to any data inside this class).
  string last_error() {
    lock_guard<mutex> lock( mutex_ );
    return last_error_;
  }

  // Should only be called by the midi thread to record its er-
  // ror.
  void set_last_error( string error ) {
    lock_guard<mutex> lock( mutex_ );
    last_error_ = std::move( error );
  }

  // Intended to be called by the main thread.
  void send_cmd( e_midi_player_cmd cmd ) {
    lock_guard<mutex> lock( mutex_ );
    commands_.push( cmd );
  }

  // Intended to be called by the midi thread.
  Opt<e_midi_player_cmd> pop_cmd() {
    lock_guard<mutex> lock( mutex_ );
    if( commands_.size() > 0 ) {
      auto ret = commands_.front();
      commands_.pop();
      return ret;
    }
    return nullopt;
  }

private:
  mutex mutex_;
  // The midi thread holds its state here and updates this when-
  // ever it changes.
  e_midi_player_state      state_{e_midi_player_state::paused};
  vector<string>           playlist_{};
  string                   last_error_{};
  queue<e_midi_player_cmd> commands_{};
};

// Shared state between main thread and midi thread.
MidiCommunication g_midi_comm;

// This data structure holds the state of a midi file that is
// being played. It will be updated on each event. It is used so
// that we can play the midi file in a non-blocking way by
// playing one event at a time and then doing other things, then
// resuming playing using the data in this struct.
struct MidiPlayInfo {
  smf::MidiFile midifile;
  int           current_event;
  double        millisecs_per_tick;
  int           tick;
};

// May fail to load the file.
Opt<MidiPlayInfo> load_midi_file( string const& file ) {
  MidiPlayInfo info;

  info.midifile.clear();
  logger->info( "loading midi file: {}\n", file );
  if( !info.midifile.read( file ) ) return nullopt;
  info.midifile.doTimeAnalysis(); // why do we need this?
  info.midifile.linkNotePairs();  // why do we need this?

  info.midifile.joinTracks();
  // Should now have precisely one track.

  auto duration_ticks = info.midifile.getFileDurationInTicks();
  auto duration_secs  = info.midifile.getFileDurationInSeconds();
  info.millisecs_per_tick =
      1000.0 * duration_secs / duration_ticks;
  info.current_event = 0;
  info.tick          = 0;
  return info;
}

// Plays a single midi event and waits first if necessary.
void midi_play_event( MidiPlayInfo& info ) {
  // Always use track 0 because on loading the midi files we join
  // all tracks into one.
  if( info.current_event >= info.midifile[0].size() ) return;
  auto const& e     = info.midifile[0][info.current_event];
  int         delta = e.tick - info.tick;
  using namespace std::chrono;
  auto duration = int( delta * info.millisecs_per_tick );
  // TODO: this could potentially block the midi thread for
  //       while.  Allow this to be broken up into smaller
  //       durations if it is e.g. larger than 100ms so that
  //       we don't block too long.
  sleep( milliseconds( duration ) );
  // Must send midi message AFTER sleeping.
  g_midi->send_midi_message( e );
  info.tick = e.tick;
  info.current_event++;
}

template<typename... Args>
void fail_midi_thread( Args... args ) {
  g_midi_comm.set_state( e_midi_player_state::failed );
  logger->warn( std::forward<Args>( args )... );
  g_midi_comm.set_last_error(
      fmt::format( std::forward<Args>( args )... ) );
}

void midi_thread_impl() {
  Opt<MidiPlayInfo>      maybe_info;
  int                    track = -1;
  Opt<e_midi_player_cmd> cmd;
  while( true ) {
    while( ( cmd = g_midi_comm.pop_cmd() ).has_value() ) {
      switch( cmd.value() ) {
        case +e_midi_player_cmd::play:
          g_midi_comm.set_state( e_midi_player_state::playing );
          break;
        case +e_midi_player_cmd::next:
          g_midi->all_notes_off();
          maybe_info = nullopt;
          g_midi_comm.set_state( e_midi_player_state::playing );
          logger->info( "skipping to next track." );
          break;
        case +e_midi_player_cmd::pause:
          g_midi->all_notes_off();
          g_midi_comm.set_state( e_midi_player_state::paused );
          break;
        case +e_midi_player_cmd::off: return;
      }
    }
    switch( g_midi_comm.state() ) {
      case +e_midi_player_state::failed: return;
      case +e_midi_player_state::off:
        logger->critical(
            "programmer error: should not be here ({})",
            __FILE__, __LINE__ );
        return;
      case +e_midi_player_state::paused:
        sleep( chrono::milliseconds( 200 ) );
        continue;
      case +e_midi_player_state::playing: {
        auto playlist = g_midi_comm.playlist();
        if( !maybe_info.has_value() ) {
          // Try loading the next track.
          if( playlist.size() == 0 ) {
            // There are no songs in the playlist, so just go
            // into the pause state.
            logger->warn(
                "no midi files in playlist; stopping player." );
            g_midi_comm.set_state( e_midi_player_state::paused );
            break;
          }
          // We have at least one track in the playlist.
          track++;
          track      = track % playlist.size();
          maybe_info = load_midi_file( playlist[track] );
          if( !maybe_info ) {
            fail_midi_thread( "failed to load midi file {}",
                              playlist[track] );
            break;
          }
          // We've loaded the midi file.
        }
        if( maybe_info.has_value() ) {
          auto& info = maybe_info.value();
          midi_play_event( info );
          // If this midi file has finished playing then signal
          // that we should load the next one.
          if( info.current_event >= info.midifile[0].size() ) {
            logger->debug( "midi file {} has finished.",
                           playlist[track] );
            // Just for good measure.
            g_midi->all_notes_off();
            maybe_info = nullopt;
          }
        }
        break;
      }
    }
  }
}

// This is the function that drives the midi thread (i.e., it is
// given to the std::thread object). When this function finishes
// so does the MIDI thread.
void midi_thread() {
  midi_thread_impl();
  logger->info( "MIDI thread exiting." );
  g_midi->all_notes_off();
  if( g_midi_comm.state() == e_midi_player_state::failed ) {
    logger->error( "MIDI thread failed: {}",
                   g_midi_comm.last_error() );
    return;
  }
  g_midi_comm.set_state( e_midi_player_state::off );
}

/****************************************************************
** Init/Cleanup
*****************************************************************/
void init_midi() {
  // Initilization of rt-midi i/o mechanism.
  auto maybe_midi_io = MidiIO::create();
  if( maybe_midi_io ) {
    g_midi.emplace( std::move( *maybe_midi_io ) );

    logger->info( "creating MIDI thread." );
    // Initialization of midi thread. This is only done if we
    // found a midi port.
    g_midi_thread = thread( []() { midi_thread(); } );
  }

  if( !g_midi.has_value() ) {
    logger->warn(
        "failed to initialize MIDI system; MIDI music will not "
        "play." );
  }
}

void cleanup_midi() {
  if( g_midi ) {
    // Cleanup midi thread.
    if( g_midi_thread.has_value() ) {
      logger->debug( "sending `off` message to midi thread" );
      g_midi_comm.send_cmd( e_midi_player_cmd::off );
      logger->debug( "waiting for midi thread to join" );
      g_midi_thread->join();
      logger->info( "MIDI thread closed." );
      // This may have already been done by the midi thread, but
      // just in case...
      g_midi->all_notes_off();
    }

    // Cleanup rt-midi i/o.
    g_midi.reset();
  }
}

} // namespace

REGISTER_INIT_ROUTINE( midi, init_midi, cleanup_midi );

/****************************************************************
** User API
*****************************************************************/
bool is_midi_playable() {
  // These are never expected to be different.
  CHECK( g_midi.has_value() ^ g_midi_thread.has_value() );

  return g_midi && g_midi_thread;
}

e_midi_player_state midi_player_state() {
  return g_midi_comm.state();
}

void send_command_to_midi_player( e_midi_player_cmd cmd ) {
  // If MIDI is not playable then just do nothing; failure of
  // MIDI to be playable can be caused by many things in the
  // players environment and, since it is not required for the
  // game, it is not a fatal error for midi to be not playable
  // and we also don't want to cloud the log files when that hap-
  // pens.  So just ignore this is midi is not playable.
  if( is_midi_playable() ) g_midi_comm.send_cmd( cmd );
}

/****************************************************************
** Testing
*****************************************************************/
// Just for testing. Assumes that port is already open.
void play_midi_file( string const& file ) {
  CHECK( g_midi );

  smf::MidiFile midifile;
  fmt::print( "File: {}\n", file );
  CHECK( midifile.read( file ) );
  midifile.doTimeAnalysis();
  midifile.linkNotePairs();

  int tracks = midifile.getTrackCount();
  fmt::print( "Tracks: {}\n", tracks );

  int const track = 0;

  midifile.joinTracks();
  fmt::print( "Tracks after join: {}\n",
              midifile.getTrackCount() );

  auto tpq = midifile.getTPQ();
  fmt::print( "Ticks Per Quarter Note: {}\n", tpq );
  auto duration_ticks = midifile.getFileDurationInTicks();
  auto duration_secs  = midifile.getFileDurationInSeconds();
  auto millisecs_per_tick =
      1000.0 * duration_secs / duration_ticks;
  fmt::print( "milliseconds per tick: {}\n",
              millisecs_per_tick );

  fmt::print( "\nTrack {}\n", track );
  fmt::print( "-----------------------------------\n" );
  fmt::print( "{:<8}{:<9}{:<10}{}\n", "Tick", "Seconds", "Dur",
              "Message" );
  fmt::print( "-----------------------------------\n" );
  int tick = 0;
  for( int event = 0; event < midifile[track].size(); event++ ) {
    fmt::print( "{:<8}", midifile[track][event].tick );
    fmt::print( "{:<9}", midifile[track][event].seconds );
    if( midifile[track][event].isNoteOn() )
      fmt::print(
          "{:<10}",
          midifile[track][event].getDurationInSeconds() );
    else
      fmt::print( "{:<10}", "" );
    for( size_t i = 0; i < midifile[track][event].size(); i++ )
      fmt::print( "{:<3x}", (int)midifile[track][event][i] );
    fmt::print( "\n" );
    auto const& e     = midifile[track][event];
    int         delta = e.tick - tick;
    using namespace std::chrono;
    auto duration = int( delta * millisecs_per_tick );
    sleep( milliseconds( duration ) );
    // Must send midi message AFTER sleeping.
    g_midi->send_midi_message( e );
    tick = e.tick;
  }
}

void test_midi() {
  if( !g_midi ) return;
  g_midi_comm.set_playlist(
      {"test.mid", "test2.mid", "test3.mid"} );
  while( true ) {
    sleep( chrono::milliseconds( 200 ) );
    if( g_midi_comm.state() == e_midi_player_state::failed ) {
      logger->info(
          "MIDI thread has failed and is no longer active." );
      break;
    }
    fmt::print( "[p]lay, [s]top, [n]ext, [q]uit]: " );
    string in;
    cin >> in;
    if( in == "p" ) {
      g_midi_comm.send_cmd( e_midi_player_cmd::play );
      continue;
    }
    if( in == "s" ) {
      g_midi_comm.send_cmd( e_midi_player_cmd::pause );
      continue;
    }
    if( in == "n" ) {
      g_midi_comm.send_cmd( e_midi_player_cmd::next );
      continue;
    }
    if( in == "q" ) break;
  }
}

} // namespace rn
