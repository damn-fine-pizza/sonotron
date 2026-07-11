#include "seqedit_model.hpp"

namespace sonotron {

void SeqEditModel::set_part_index(std::size_t index) {
  m_part_index = index < kTrackRoleCount ? index : kTrackRoleCount - 1;
}

}  // namespace sonotron
