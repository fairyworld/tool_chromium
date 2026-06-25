// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/glic_actuation_priority_voter.h"

#include <utility>

#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContextRegistry::GetFromGraph(
             frame_node->GetGraph())
      ->GetExecutionContextForFrameNode(frame_node);
}

}  // namespace

// static
const char GlicActuationPriorityVoter::kGlicActuationReason[] =
    "Glic task actuation.";

GlicActuationPriorityVoter::GlicActuationPriorityVoter() = default;
GlicActuationPriorityVoter::~GlicActuationPriorityVoter() = default;

void GlicActuationPriorityVoter::InitializeOnGraph(
    Graph* graph,
    VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void GlicActuationPriorityVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  voting_channel_.Reset();
}

void GlicActuationPriorityVoter::OnIsGlicActuatingChanged(
    const PageNode* page_node) {
  // This function is guaranteed to be called only when the actuation state
  // changes, so we don't need to track whether the vote has already been cast.
  const bool is_actuating =
      PageLiveStateDecorator::Data::FromPageNode(page_node)->IsGlicActuating();

  if (auto* main_frame_node = page_node->GetMainFrameNode()) {
    UpdateFrameNodeVote(main_frame_node, is_actuating);
  }
}

void GlicActuationPriorityVoter::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
}

void GlicActuationPriorityVoter::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
}

void GlicActuationPriorityVoter::OnBeforeFrameNodeAdded(
    const FrameNode* frame_node,
    const FrameNode* pending_parent_frame_node,
    const PageNode* pending_page_node,
    const ProcessNode* pending_process_node,
    const FrameNode* pending_parent_or_outer_document_or_embedder) {
  const bool is_actuating =
      PageLiveStateDecorator::Data::FromPageNode(pending_page_node)
          ->IsGlicActuating();
  if (is_actuating) {
    UpdateFrameNodeVote(frame_node, is_actuating);
  }
}

void GlicActuationPriorityVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  const bool is_actuating =
      PageLiveStateDecorator::Data::FromPageNode(frame_node->GetPageNode())
          ->IsGlicActuating();
  if (is_actuating) {
    UpdateFrameNodeVote(frame_node, /*is_actuating=*/false);
  }
}

void GlicActuationPriorityVoter::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  // An actuating glic instance should never have its current frame changed.
  CHECK(!PageLiveStateDecorator::Data::FromPageNode(
             current_frame_node->GetPageNode())
             ->IsGlicActuating());
}

void GlicActuationPriorityVoter::UpdateFrameNodeVote(
    const FrameNode* frame_node,
    bool is_actuating) {
  // Only the main frame(s) participate in actuation.
  if (!frame_node->IsMainFrame()) {
    return;
  }
  if (is_actuating) {
    voting_channel_.SubmitVote(
        GetExecutionContext(frame_node),
        Vote(base::Process::Priority::kUserBlocking, kGlicActuationReason));
  } else {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
}

}  // namespace performance_manager::execution_context_priority
