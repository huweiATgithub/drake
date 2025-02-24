#pragma once

#include <map>
#include <string>
#include <utility>

#include "optitrack/optitrack_frame_t.hpp"

#include "drake/common/drake_copyable.h"
#include "drake/common/drake_deprecated.h"
#include "drake/geometry/geometry_ids.h"
#include "drake/systems/framework/leaf_system.h"

namespace drake {
namespace systems {
namespace sensors {

/// The system has one abstract-valued input port and one abstract-valued output
/// port. The one abstract input port accepts geometry::FramePoseVector.  The
/// abstract output port produces `optitrack_frame_t`.
///
/// Note that this system does not actually send this message on an LCM channel.
/// To send the message, the output of this system should be connected to an
/// input port of a Value object templated on type `optitrack_frame_t`.
///
/// @system
/// name: OptitrackLcmFrameSender
/// input_ports:
/// - u0
/// output_ports:
/// - y0
/// @endsystem
class DRAKE_DEPRECATED(
    "2023-11-01",
    "The OptitrackLcmFrameSender will be removed from Drake. If you still need "
    "to use this class, feel free to copy its source code to your own project "
    "and customize it to your needs.")  // BR
    OptitrackLcmFrameSender : public LeafSystem<double> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(OptitrackLcmFrameSender)

  // TODO(sam.creasey) Support publising data description messages.
  /// Create an OptitrackLcmFrameSender.
  ///
  /// @param frame_map contains a map of geometry::FrameId to the rigid body
  /// name and id to be published in the resulting `optitrack_rigid_body_t` and
  /// `optitrack_rigid_body_description_t` messages (descriptions not yet
  /// implemented).
  explicit OptitrackLcmFrameSender(
      const std::map<geometry::FrameId, std::pair<std::string, int>>&
          frame_map);

  const systems::InputPort<double>& get_optitrack_input_port() const {
    return get_input_port(pose_input_port_index_);
  }

  const systems::OutputPort<double>& get_lcm_output_port() const {
    return get_output_port(0);
  }

 private:
  void PopulatePoseMessage(const systems::Context<double>& context,
                           optitrack::optitrack_frame_t* output) const;

  const int num_rigid_bodies_;
  const std::map<geometry::FrameId, std::pair<std::string, int>> frame_map_;
  int pose_input_port_index_{-1};
};

}  // namespace sensors
}  // namespace systems
}  // namespace drake
