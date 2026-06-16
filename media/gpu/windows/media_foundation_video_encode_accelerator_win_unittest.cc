// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/encoder_status.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/win/dxgi_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace media {

namespace {

class MockEncoderClient : public VideoEncodeAccelerator::Client {
 public:
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {}
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {}
  void NotifyErrorStatus(const EncoderStatus& status) override {
    status_ = status;
  }
  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override {}

  EncoderStatus GetLastStatus() const { return status_; }

 private:
  EncoderStatus status_ = EncoderStatus::Codes::kOk;
};

class TestMediaFoundationVideoEncodeAccelerator
    : public MediaFoundationVideoEncodeAccelerator {
 public:
  using MediaFoundationVideoEncodeAccelerator::InitializeForTesting;

  TestMediaFoundationVideoEncodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      CHROME_LUID luid)
      : MediaFoundationVideoEncodeAccelerator(gpu_preferences,
                                              gpu_workarounds,
                                              luid) {}
  ~TestMediaFoundationVideoEncodeAccelerator() override = default;
};

}  // namespace

class MediaFoundationVideoEncodeAcceleratorTest : public ::testing::Test {
 protected:
  void SetUpFakeEncoder(TestMediaFoundationVideoEncodeAccelerator* encoder,
                        VideoEncodeAccelerator::Client* client,
                        Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
    encoder->InitializeForTesting(
        client, std::make_unique<NullMediaLog>(), gfx::Size(1920, 1080),
        DXGIDeviceManager::Create(CHROME_LUID{0, 0}, d3d11_device.Get()));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

class MediaFoundationVideoEncodeAcceleratorAlignmentTest
    : public MediaFoundationVideoEncodeAcceleratorTest,
      public ::testing::WithParamInterface<VideoPixelFormat> {};

TEST_P(MediaFoundationVideoEncodeAcceleratorAlignmentTest,
       RejectUnalignedVisibleRect) {
  // Step 1: Initialize a D3D11 device. Try hardware first, then fallback to
  // WARP (software) if unavailable.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                        0, D3D11_SDK_VERSION, &d3d11_device, nullptr, &context);
  if (FAILED(hr)) {
    hr =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                          D3D11_SDK_VERSION, &d3d11_device, nullptr, &context);
    if (FAILED(hr)) {
      GTEST_SKIP() << "D3D11 device creation failed";
    }
  }

  // Step 2: Set up a fake MediaFoundationVideoEncodeAccelerator with a mock
  // client.
  auto encoder = base::WrapUnique(new TestMediaFoundationVideoEncodeAccelerator(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
      CHROME_LUID{0, 0}));

  MockEncoderClient client;
  SetUpFakeEncoder(encoder.get(), &client, d3d11_device);

  // Step 3: Define an intentionally unaligned visible_rect (odd
  // coordinates/dimensions) for a 4:2:0 format.
  gfx::Rect visible_rect(1, 1, 1919, 1079);
  gfx::Size natural_size(1919, 1079);
  VideoPixelFormat format = GetParam();
  scoped_refptr<VideoFrame> frame;

  if (format == PIXEL_FORMAT_NV12) {
    // Step 4a: For NV12, simulate a GPU-backed frame. Create a shared D3D11
    // texture and wrap it in a SharedImage and VideoFrame.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1920;
    desc.Height = 1080;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = d3d11_device->CreateTexture2D(&desc, nullptr, &texture);
    ASSERT_HRESULT_SUCCEEDED(hr);

    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = texture.As(&dxgi_resource);
    ASSERT_HRESULT_SUCCEEDED(hr);

    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                           nullptr, &shared_handle);
    ASSERT_HRESULT_SUCCEEDED(hr);

    gfx::GpuMemoryBufferHandle gmb_handle{
        gfx::DXGIHandle(base::win::ScopedHandle(shared_handle))};
    gmb_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;

    auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    auto shared_image = test_sii->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, gfx::Size(1920, 1080), gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_DISPLAY_READ,
         "MediaFoundationVideoEncodeAcceleratorTest"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
        std::move(gmb_handle));

    frame = VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), test_sii->GenVerifiedSyncToken(),
        base::DoNothing(), visible_rect, natural_size, base::TimeDelta());
  } else {
    // Step 4b: For memory-backed frames (I420, YV12, NV21), use
    // CreateZeroInitializedFrame instead of D3D texture mocks.
    frame = VideoFrame::CreateZeroInitializedFrame(
        format, gfx::Size(1920, 1080), visible_rect, natural_size,
        base::TimeDelta());
  }

  if (!frame) {
    // Step 5: Check if the frame was successfully created.
    // VideoFrame::CreateZeroInitializedFrame natively validates alignment and
    // might return null, in which case we safely skip.
    GTEST_SKIP() << "Cannot create unaligned frame natively for format "
                 << format;
  }

  encoder->Encode(frame, false);
  task_environment_.RunUntilIdle();

  // Step 6: Attempt to encode the frame. Because the visible_rect is unaligned,
  // the encoder should immediately reject the frame and safely report
  // kInvalidInputFrame via the client.
  EXPECT_EQ(client.GetLastStatus().code(),
            EncoderStatus::Codes::kInvalidInputFrame);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaFoundationVideoEncodeAcceleratorAlignmentTest,
                         ::testing::Values(PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_YV12,
                                           PIXEL_FORMAT_NV21));

}  // namespace media
