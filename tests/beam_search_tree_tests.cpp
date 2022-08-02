// @author Nikolay Malkovsky 2022--...

#include "beam_search_tree.h"

#include <catch2/catch.hpp>

using beam_search::CircularArrayCTCBeamSearchTree;

struct EmptyBeamEntry {};

TEST_CASE("Circular array CTC beam search tree test") {
  /**
   *                             -> (3, 0)  -> (6, 3) -> (8, 0) -> (9, 5)
   *                           /          /
   *                 -> (1, 1)  -> (4, 2) -> (7, 4) -> (10, 0) -> (11, 5) -> (12, 6)
   *               /
   * root -> (0, 0)
   *              \
   *               -> (2, 10) -> (5, 1) -> (13, 2) -> (14, 4)
   */
   CircularArrayCTCBeamSearchTree<EmptyBeamEntry> tree(16);

  auto root = tree.InitializeTree();
  std::vector<beam_search::IndexType> active_entries;
  bool* created;
  active_entries.push_back(tree.GetChild(root, 0, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[0], 1, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[0], 10, created));
  CHECK(*created == true);
  tree.GetChild(active_entries[0], 1, created);
  CHECK(*created == false);

  active_entries.push_back(tree.GetChild(active_entries[1], 0, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[1], 2, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[2], 1, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[4], 3, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[4], 4, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[6], 0, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[8], 5, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[7], 0, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[10], 5, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[11], 6, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[5], 2, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[13], 4, created));
  CHECK(*created == true);

  std::vector<beam_search::LabelType> reference = {0, 1, 2, 4, 0, 5, 6};
  CHECK(std::equal(reference.begin(), reference.end(), tree.BacktraceString(active_entries[12]).begin()));

  /**
   * At this point capacity is reached
   */
  auto label_should_fail = tree.GetChild(active_entries[14], 2, created);
  CHECK(label_should_fail == beam_search::kNoIndex);

  /**
   * Clearing everything except (12, 6)
   */
  tree.DeleteEntry(root);
  beam_search::IndexType main_path = active_entries[12];
  for (auto entry: active_entries) {
    if (entry != main_path) {
      tree.DeleteEntry(entry);
    }
  }
  active_entries.clear();
  active_entries.push_back(main_path);
  CHECK(std::equal(reference.begin(), reference.end(), tree.BacktraceString(main_path).begin()));
  // 12 and deleted 13, 14
  CHECK(tree.GetSize() == 3);
  /**
   *                             -> (3, 0)
   *                           /
   *                 -> (1, 1)  -> (4, 2)
   *               /
   *        (12, 6)
   *              \
   *               -> (2, 10) -> (5, 1)
   */
  active_entries.push_back(tree.GetChild(active_entries[0], 1, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[0], 10, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[1], 0, created));
  CHECK(*created == true);
  active_entries.push_back(tree.GetChild(active_entries[1], 2, created));
  CHECK(*created == true);

  active_entries.push_back(tree.GetChild(active_entries[2], 1, created));
  CHECK(*created == true);

  reference.push_back(1);
  reference.push_back(2);
  CHECK(std::equal(reference.begin(), reference.end(), tree.BacktraceString(active_entries[4]).begin()));

  // Current size of the tree + 2 entries with indecices 14, 15 deleted earlier but still presented in the tree due to
  // implementation
  CHECK(tree.GetSize() == 8);
  tree.DeleteEntry(active_entries[0]);
  tree.DeleteEntry(active_entries[1]);
  tree.DeleteEntry(active_entries[3]);
  CHECK(tree.GetSize() == 8);
  tree.DeleteEntry(active_entries[4]);
  // Entries 2, 5 and deleted 3, 4
  CHECK(tree.GetSize() == 4);
  tree.DeleteEntry(active_entries[2]);
  CHECK(tree.GetSize() == 1);
}