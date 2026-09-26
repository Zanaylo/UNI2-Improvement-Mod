#include "Core/Harness/FrameGrab.h"

#include "Core/ComRef.h"
#include "Core/Formats/PngImage.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

struct Frame
{
	int width = 0;
	int height = 0;
	std::vector<uint8_t> bgra;
	std::string error;
};

std::atomic<bool> g_wanted{ false };

std::mutex g_lock;
std::condition_variable g_ready;
bool g_done = false;
Frame g_frame;

bool IsReadableFormat(D3DFORMAT format)
{
	return format == D3DFMT_X8R8G8B8 || format == D3DFMT_A8R8G8B8;
}

bool Fail(Frame& frame, const char* what, HRESULT result)
{
	char text[96] = {};
	sprintf_s(text, "%s failed (0x%08lx)", what, static_cast<unsigned long>(result));
	frame.error = text;
	return false;
}

bool CopyRows(IDirect3DSurface9* surface, const D3DSURFACE_DESC& desc, Frame& frame)
{
	D3DLOCKED_RECT locked = {};
	const HRESULT result = surface->LockRect(&locked, nullptr, D3DLOCK_READONLY);
	if (FAILED(result))
		return Fail(frame, "LockRect", result);

	frame.width = static_cast<int>(desc.Width);
	frame.height = static_cast<int>(desc.Height);
	frame.bgra.resize(static_cast<size_t>(desc.Width) * desc.Height * 4);

	const size_t rowBytes = static_cast<size_t>(desc.Width) * 4;

	for (UINT row = 0; row < desc.Height; ++row)
	{
		memcpy(&frame.bgra[row * rowBytes], static_cast<const uint8_t*>(locked.pBits) + row * locked.Pitch,
			rowBytes);
	}

	surface->UnlockRect();
	return true;
}

bool ReadBackBuffer(IDirect3DDevice9* device, Frame& frame)
{
	ComRef<IDirect3DSurface9> back;
	HRESULT result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, back.Out());
	if (FAILED(result))
		return Fail(frame, "GetBackBuffer", result);

	D3DSURFACE_DESC desc = {};
	back->GetDesc(&desc);

	if (!IsReadableFormat(desc.Format))
	{
		char text[64] = {};
		sprintf_s(text, "back buffer format %d is not 32-bit RGB", static_cast<int>(desc.Format));
		frame.error = text;
		return false;
	}

	IDirect3DSurface9* source = back.Get();
	ComRef<IDirect3DSurface9> resolved;

	if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		result = device->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
			resolved.Out(), nullptr);
		if (FAILED(result))
			return Fail(frame, "CreateRenderTarget", result);

		result = device->StretchRect(back.Get(), nullptr, resolved.Get(), nullptr, D3DTEXF_NONE);
		if (FAILED(result))
			return Fail(frame, "StretchRect", result);

		source = resolved.Get();
	}

	ComRef<IDirect3DSurface9> copy;
	result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
		copy.Out(), nullptr);
	if (FAILED(result))
		return Fail(frame, "CreateOffscreenPlainSurface", result);

	result = device->GetRenderTargetData(source, copy.Get());
	if (FAILED(result))
		return Fail(frame, "GetRenderTargetData", result);

	return CopyRows(copy.Get(), desc, frame);
}

bool WritePng(const std::string& path, const Frame& frame, std::string& outReply)
{
	std::vector<uint8_t> png;
	if (!PngImage::Encode(frame.width, frame.height, frame.bgra, png))
	{
		outReply = "error png encode failed";
		return false;
	}

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
	{
		outReply = "error cannot write " + path;
		return false;
	}

	const bool written = fwrite(png.data(), 1, png.size(), file) == png.size();
	fclose(file);

	if (!written)
	{
		outReply = "error short write " + path;
		return false;
	}

	char text[48] = {};
	sprintf_s(text, "ok %dx%d ", frame.width, frame.height);
	outReply = text + path;
	return true;
}

}

bool FrameGrab::Capture(const std::string& path, DWORD timeoutMs, std::string& outReply)
{
	Frame frame;

	{
		std::unique_lock<std::mutex> lock(g_lock);

		g_done = false;
		g_wanted.store(true, std::memory_order_release);

		if (!g_ready.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return g_done; }))
		{
			g_wanted.store(false, std::memory_order_release);
			outReply = "error no frame was presented in time (is the window minimised?)";
			return false;
		}

		frame = std::move(g_frame);
	}

	if (!frame.error.empty())
	{
		outReply = "error " + frame.error;
		return false;
	}

	return WritePng(path, frame, outReply);
}

void FrameGrab::OnPresent(IDirect3DDevice9* device)
{
	if (!g_wanted.exchange(false, std::memory_order_acq_rel))
		return;

	Frame frame;
	ReadBackBuffer(device, frame);

	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_frame = std::move(frame);
		g_done = true;
	}

	g_ready.notify_all();
}
