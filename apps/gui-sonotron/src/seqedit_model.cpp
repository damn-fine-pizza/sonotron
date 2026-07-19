#include "seqedit_model.hpp"

namespace sonotron {

void SeqEditModel::set_part_index(std::size_t index) {
  m_part_index = index < kTrackRoleCount ? index : kTrackRoleCount - 1;
}

void SeqEditModel::set_all_tracks_visible(bool all) {
  m_all_tracks_shown = all;
  if (all) {
    m_role_visible.fill(true);
  } else {
    m_role_visible.fill(false);
    m_role_visible[m_part_index] = true;
  }
}

}  // namespace sonotron
