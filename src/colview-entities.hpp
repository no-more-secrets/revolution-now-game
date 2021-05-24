/****************************************************************
**colview-entities.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2020-01-12.
*
* Description: The various UI sections/entities in Colony view.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "cargo.hpp"
#include "commodity.hpp"
#include "coord.hpp"
#include "dragdrop.hpp"
#include "id.hpp"
#include "tx.hpp"
#include "view.hpp"
#include "waitable.hpp"

// Rnl
#include "rnl/colview-entities.hpp"

namespace rn {

// TODO: Keep this generic and move it into the ui namespace
// eventually.
struct AwaitableView {
  virtual ~AwaitableView() = default;

  virtual waitable<> perform_click( Coord ) {
    return make_waitable<>();
  }
};

class ColonySubView;

struct PositionedColSubView {
  ColonySubView* col_view;
  Coord          upper_left;
};

struct ColViewObjectWithBounds {
  ColViewObject_t obj;
  Rect            bounds;
};

/****************************************************************
** Drag/Drop Interfaces
*****************************************************************/

// Interface for views that support prompting a user for informa-
// tion on the parameters of a drag.
struct IColViewDragSourceUserInput {
  // This will only be called if the user requests it, typically
  // by holding down a modifier key such as shift when releasing
  // the drag. Using this method, the view has the opportunity to
  // edit the dragged object before it is proposed to the drag
  // target view. If it edits it, then it will update its in-
  // ternal record. Either way, it will return the current object
  // being dragged after editing (which could be unchanged). If
  // it returns `nothing` then the drag is considered to be can-
  // celled.
  virtual waitable<maybe<ColViewObject_t>> user_edit_object()
      const = 0;
};

// Interface for views that can be the source for dragging. The
// idea here is that the view must keep track of what is being
// dragged.
struct IColViewDragSource {
  // If this returns something other than nothing, this means
  // that the view has recorded the object and assumes that the
  // drag has begun. Note: this function may be called when a
  // drag is already in progress in order to adjust the object
  // being dragged.
  virtual bool try_drag( ColViewObject_t const& o ) = 0;

  // This function must be called if the drag is cancelled for
  // any reason before it is affected. It is recommended to call
  // this function in a SCOPE_EXIT just after calling try_drag.
  virtual void cancel_drag() = 0;

  // This is used to indicate whether the user can hold down a
  // modifier key while releasing the drag to signal that they
  // wish to be prompted and asked for information to customize
  // the drag, such as e.g. specifying the amount of a commodity
  // to be dragged.
  maybe<IColViewDragSourceUserInput const&> drag_user_input()
      const;

  // This will permanently remove the object from the source
  // view's ownership, so should only be called just before the
  // drop is to take effect.
  virtual void disown_dragged_object() = 0;
};

// Interface for drag targets that can/might ask the user for
// confirmation just before affecting the drag.
struct IColViewDragSinkConfirm {
  virtual waitable<bool> confirm( ColViewObject_t const&,
                                  Coord const& ) const = 0;
};

// Interface for views that can accept dragged items.
struct IColViewDragSink {
  // Coordinates are relative to view's upper left corner. If the
  // object can be received, then a new object will be returned
  // that is either the same or possibly altered from the origi-
  // nal, for example if the user is moving commodities into a
  // ship's cargo and the cargo only has room for half of the
  // quantity that the user is dragging, the returned object will
  // be updated to reflect the capacity of the cargo so that the
  // controller algorithm knows how much to remove from source.
  virtual maybe<ColViewObject_t> can_receive(
      ColViewObject_t const& o, Coord const& where ) const = 0;

  maybe<IColViewDragSinkConfirm const&> drag_confirm() const;

  // Coordinates are relative to view's upper left corner. Re-
  // turns true if the drop succeeded. If the drop succeeds then
  // source.take_object() MUST be called on the source.
  virtual void drop( ColViewObject_t const& o,
                     Coord const&           where ) = 0;
};

class ColonySubView : public AwaitableView {
public:
  ColonySubView() = default;

  // virtual e_colview_entity entity_id() const = 0;

  // All ColonySubView's will also be unspecified subclassess of
  // ui::View.
  virtual ui::View&       view() noexcept       = 0;
  virtual ui::View const& view() const noexcept = 0;

  // Coordinate will be relative to the upper-left of the view.
  // Should only be called if the coord is within the bounds of
  // the view.
  virtual maybe<PositionedColSubView> view_here( Coord ) {
    return PositionedColSubView{ this, Coord{} };
  }

  virtual maybe<ColViewObjectWithBounds> object_here(
      Coord const& /*where*/ ) const {
    return nothing;
  }

  // For convenience.
  maybe<IColViewDragSource&> drag_source();
  maybe<IColViewDragSink&>   drag_sink();

private:
  ColonyId id_;
};

// The pointer returned from these will be invalidated if
// set_colview_colony is called with a new colony id.
ColonySubView& colview_entity( e_colview_entity entity );
ColonySubView& colview_top_level();

// Must be called before any other method in this module.
void set_colview_colony( ColonyId id );

void colview_drag_n_drop_draw(
    drag::State<ColViewObject_t> const& state, Texture& tx );

} // namespace rn
