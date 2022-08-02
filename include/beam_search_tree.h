// @author Nikolay Malkovsky 2022--...

#include <vector>
#include <limits>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace beam_search {

using IndexType = uint32_t;
using CounterType = IndexType;
using LabelType = uint16_t;
const IndexType kNoIndex = std::numeric_limits<IndexType>::max();
const LabelType kNoLabel = std::numeric_limits<LabelType>::max();

template<class BeamEntry>
class CircularArrayCTCBeamEntryInternal {
 public:
  CircularArrayCTCBeamEntryInternal() = default;
  CircularArrayCTCBeamEntryInternal(LabelType label, IndexType parent, BeamEntry &&entry) : label_(label),
                                                                                            entry_(std::move(entry)),
                                                                                            parent_(parent) {}

  template<class... Args>
  CircularArrayCTCBeamEntryInternal(LabelType label, IndexType parent, Args &&... args) : label_(label),
                                                                                          parent_(parent), entry_(
      std::forward(args)...) {}

  /**
   * Returns mutable reference to an origin BeamEntry
   * @return BeamEntry associated with the entry
   */
  BeamEntry &GetEntry() { return entry_; }

  /**
   * Adds a reference to an entry
   */
  void AddEntryReference() { ++reference_count_; }

  /**
   * Marks that one of the entry was deleted. Entry itself will not be deleted right away though and can be accessed until
   * all its predecessors and children are also deleted.
   */
  void DeleteEntryReference() {
    if (reference_count_ == 0) {
      std::runtime_error("Attempted deletion of an entry with no references!!!");
    }
    --reference_count_;
  }

  /**
   * Marks the entry to be inactive, meaning that it is not currently used by beam search algorithm until
   * returned by GetChild or removed/detached completely.
   */
  void MarkInactive() {
    active_ = false;
  }

  /**
   * Marks entry as active, meaning that it was previously marked inactive and now being accessed by GetChild
   */
  void MarkActive() {
    active_ = true;
  }

  /**
   * Returns true is the state was not discarded by beam search via DeleteEntry since last GetChild
   */
  bool IsActive() {
    return active_;
  }

  /**
   * Number of references to an entry
   * @return
   */
  const CounterType ReferenceCount() const { return reference_count_; }

  /**
   * Returns an index to a first child, i.e. the head of the children list
   * @return
   */
  IndexType GetFirstChild() const { return first_child_; }

  /**
   * Sets first child
   */
  void SetFirstChild(IndexType value) { first_child_ = value; }

  /**
   * Returns an index to a sibling, i.e. the next child in the list of children for current entry's parent
   * @return
   */
  IndexType GetSibling() const { return sibling_; }

  /**
   * Sets the sibling
   */
  void SetSibling(IndexType value) { sibling_ = value; }

  /**
   * Returns an index of a parent entry
   */
  IndexType GetParent() { return parent_; }

  /**
   * Returns a label corresponding to this entry
   */
  LabelType GetLabel() const { return label_; }

  /**
   * Cut off the history of the entry
   */
  void MakeRoot() {
    parent_ = kNoIndex;
  }

  /**
   * @return true if the entry is beam search root
   */
  bool IsRoot() {
    return parent_ == kNoIndex;
  }

 private:
  // GC members, we need an additional "actity" flag to track the situation when the active entry
  // is an LCA of all the others active entries and the entry has only child.
  CounterType reference_count_ = 1;
  bool active_ = true;
  /*
   * Children map members.
   *
   * Note. There are a lot of deletions in children maps, average number of elements in children is relatively
   * low. It is not recommended to use std containers here as they will lead to double allocations, inplace
   * self-implemented structures are preferred, default choice is inplace list.
   */
  IndexType first_child_ = kNoIndex;
  IndexType sibling_ = kNoIndex;
  LabelType label_;

  IndexType parent_;

  BeamEntry entry_;
};

template<class BeamEntry>
class DetachedSharedPrefixBeamEntry {
 public:
  DetachedSharedPrefixBeamEntry(LabelType label, const BeamEntry& entry) : label_(label), entry_(entry) {}

  DetachedSharedPrefixBeamEntry(LabelType label, BeamEntry&& entry) : label_(label), entry_(std::move(entry)) {}

  LabelType label_;
  const BeamEntry entry_;
};

/**
 * Implementation of a beam search tree data structure. It consists of a prefix tree with custom allocator designed
 * specifically for beam search.
 *
 * The allocator is circular array that takes advantage of beam search entries creation topology, entries deletion
 * is based upon reference counting but in contrast with traditional algorithms it doesn't delete an entry until
 * all it's predecessors are also deleted.
 *
 * To make the best out of this implementation it is not recommended to use pointers as BeamEntry as that would
 * delegate memory management to a general allocator.
 * @tparam BeamEntry structure to track non-topologic beam search information
 */
template<class BeamEntry>
class CircularArrayCTCBeamSearchTree {
 public:
  /**
   * Initializes beam search tree.
   * @param capacity maximum number of elements for tree to store. Attempting the allocation of entry
   * beyond the capacity limit will fail.
   */
  CircularArrayCTCBeamSearchTree(IndexType capacity) {
    IndexType capacity_padded = 1;
    while (capacity_padded < capacity) {
      capacity_padded <<= 1;
    }
    capacity_ = capacity_padded;
    entries_.resize(capacity_);
  }

  /**
   * Initializes beam search tree and returns the index of a root entry
   * @param entry BeamEntry to assign to root
   * @return root entry index
   */
  template<class... Args>
  IndexType InitializeTree(Args&&... args) {
    entries_[right_] = CircularArrayCTCBeamEntryInternal<BeamEntry>(kNoLabel, kNoIndex);
    ++size_;
    ++right_;
    return 0;
  }


  /**
   * Reinitialize the tree
   * @return index of the root of the new tree
   */
  template<class... Args>
  IndexType Reset(Args&&... args) {
    left_ = 0;
    right_ = 0;
    size_ = 0;
    entries_.clear();
    detached_shared_prefix_.clear();
    return InitializeTree(args...);
  }

  std::vector<CircularArrayCTCBeamEntryInternal<BeamEntry>> Backtrace(IndexType entry_index) {
    std::vector<CircularArrayCTCBeamEntryInternal<BeamEntry>> result;
    while (!entries_[entry_index].IsRoot()) {
      result.push_back(entries_[entry_index]);
      entry_index = entries_[entry_index].GetParent();
    }
    for (auto entry_iter = detached_shared_prefix_.rbegin();
         entry_iter != detached_shared_prefix_.rend(); ++entry_iter) {
      result.push_back(*entry_iter);
    }
    return std::vector<CircularArrayCTCBeamEntryInternal<BeamEntry>>(result.rbegin(), result.rend());
  }

  std::vector<LabelType> BacktraceString(IndexType entry_index) {
    std::vector<LabelType> result;
    while (!entries_[entry_index].IsRoot()) {
      if (entries_[entry_index].GetLabel() != kNoLabel) {
        result.push_back(entries_[entry_index].GetLabel());
      }
      entry_index = entries_[entry_index].GetParent();
    }
    if (entries_[entry_index].GetLabel() != kNoLabel) {
      result.push_back(entries_[entry_index].GetLabel());
    }
    for (auto entry_iter = detached_shared_prefix_.rbegin();
         entry_iter != detached_shared_prefix_.rend(); ++entry_iter) {
      if (entry_iter->label_ != kNoLabel) {
        result.push_back(entry_iter->label_);
      }
    }
    return std::vector<LabelType>(result.rbegin(), result.rend());
  }

  /**
   * Tell the beam search tree that the entry is no longer in use by beam search. The entry will remain until
   * all its predecessors are also deleted or it is requested by GetChild
   * @param index index of the entry to be deleted
   */
  void DeleteEntry(IndexType index) {
    entries_[index].MarkInactive();
    entries_[index].DeleteEntryReference();
    while (entries_[index].ReferenceCount() == 0) {
      index = entries_[index].GetParent();
      if (index == kNoIndex) {
        break;
      }
      entries_[index].DeleteEntryReference();
    }
    /**
     * Root/LCA should be left in the tree
     */
    while (entries_[left_].ReferenceCount() <= 1 and !entries_[left_].IsActive()) {
      // This is the case for shared prefix entry
      if (entries_[left_].ReferenceCount() == 1) {
        detached_shared_prefix_.emplace_back(entries_[left_].GetLabel(), entries_[left_].GetEntry());
      }
      left_ = (left_ + 1) & (capacity_ - 1);
      --size_;
    }
    entries_[left_].MakeRoot();
  }

  /**
   * Gets existing child of parent with corresponding label or creating new one. If creation is required when capacity
   * is reached, no changes occur and kNoIndex is returned.
   * @param parent parent label in the tree
   * @param label label of requested child
   * @param created store true if the child was created by the method, false otherwise
   * @return Index of the child if successfully found or created, kNoIndex otherwise
   */
  IndexType GetChild(IndexType parent, LabelType label, bool *created) {
    for (auto cur = entries_[parent].GetFirstChild(); cur != kNoIndex; cur = entries_[cur].GetSibling()) {
      if (entries_[cur].GetLabel() == label) {
        *created = false;
        entries_[cur].MarkActive();
        return cur;
      }
    }
    *created = true;
    if (size_ > 0 and right_ == left_) {
      return kNoIndex;
    }
    auto result = right_;
    entries_[right_] = CircularArrayCTCBeamEntryInternal<BeamEntry>(label, parent);
    entries_[right_].SetSibling(entries_[parent].GetFirstChild());
    entries_[parent].SetFirstChild(right_);
    entries_[parent].AddEntryReference();
    right_ = (right_ + 1) & (capacity_ - 1);
    size_++;
    return result;
  }

  /**
   * Gets the current size of the tree without shared prefix. LCA of the current branches is included in the tree as root
   * @return size of the tree
   */
  const IndexType GetSize() const { return size_; }

 private:
  IndexType left_ = 0;
  IndexType right_ = 0;
  IndexType size_ = 0;
  IndexType capacity_;
  std::vector<CircularArrayCTCBeamEntryInternal<BeamEntry>> entries_;
  std::vector<DetachedSharedPrefixBeamEntry<BeamEntry>> detached_shared_prefix_;
};

} // beam_search
