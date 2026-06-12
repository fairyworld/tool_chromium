// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_image_extractor.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

namespace {

void OnGotImageBytes(
    base::ElapsedTimer elapsed_timer,
    mojo::Remote<blink::mojom::AIPageContentAgent> agent,
    base::OnceCallback<void(blink::mojom::AIPageContentImageBytesResultPtr)>
        callback,
    blink::mojom::AIPageContentImageBytesResultPtr result) {
  if (result) {
    base::UmaHistogramTimes(
        "OptimizationGuide.AIPageContent.GetImageBytes.Latency",
        elapsed_timer.Elapsed());
    base::UmaHistogramCounts10M(
        "OptimizationGuide.AIPageContent.GetImageBytes.Size",
        base::saturated_cast<int>(result->image_bytes.size()));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

void GetImageBytes(
    content::WebContents* web_contents,
    const std::string& document_identifier,
    int32_t dom_node_id,
    base::OnceCallback<
        void(blink::mojom::AIPageContentImageBytesResultPtr result)> callback) {
  CHECK(web_contents);
  CHECK(callback);

  content::RenderFrameHost* target_rfh =
      GetRenderFrameForDocumentIdentifier(*web_contents, document_identifier);

  if (!target_rfh) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  const content::RenderWidgetHost* target_rwh =
      target_rfh->GetRenderWidgetHost();
  if (!target_rwh) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  // The `AIPageContentAgent` Mojo interface is only registered on local root
  // frames in the renderer. We must find the local root `RenderFrameHost` that
  // contains `target_rfh` and bind to its agent. Since DOM node IDs are
  // process-wide unique, the local root agent can successfully look up and
  // extract bytes for any node belonging to its subframes.
  content::RenderFrameHost* local_root = target_rfh;
  while (true) {
    content::RenderFrameHost* parent =
        local_root->GetParentOrOuterDocumentOrEmbedder();
    if (!parent || parent->GetRenderWidgetHost() != target_rwh) {
      break;
    }
    local_root = parent;
  }

  mojo::Remote<blink::mojom::AIPageContentAgent> agent;
  local_root->GetRemoteInterfaces()->GetInterface(
      agent.BindNewPipeAndPassReceiver());

  // TODO(b/513285640): Consider implementing a browser-side timeout to
  // prevent the request from hanging indefinitely.
  auto* agent_ptr = agent.get();
  agent_ptr->GetImageBytes(
      dom_node_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                       base::BindOnce(&OnGotImageBytes, base::ElapsedTimer(),
                                      std::move(agent), std::move(callback)),
                       /*result=*/nullptr));
}

}  // namespace optimization_guide
