#include "stdafx.h"
#include "Base3DRenderer.h"

#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#define DLL2_EXPORTS
#include <stdio.h> 

namespace DSObjects 
{
	Base3DRenderer::Base3DRenderer(IDirect3DDevice9* device, const CSize &monitorSize)
	{
		this->textures = 0;
		this->surfaces = 0;
		this->textureHeight = this->textureWidth = -1;
		this->device = device;

		this->textureIndexWrite = 0;
		this->textureIndexWrite = 0;

		this->swapLock = new CCriticalSection();

		if (mode == 1) 
		{
			// Start thread
			AfxBeginThread(StartRenderThread, this, THREAD_PRIORITY_TIME_CRITICAL);
		}
	}
	
	void Base3DRenderer::Render() 
	{
		//this->RenderFrame(textures, 0); -> done async

		if (mode == 0) 
		{
			CComPtr<IDirect3DSurface9> pBackBuffer;
			device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

			// Clear the backbuffer
			device->SetRenderTarget(0, pBackBuffer);
			device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF0000, 1.0f, 0);

			// Render the frame!
			this->RenderFrame(textures, this->textureIndexWrite);

			// Draw on the window
			device->Present(0,0,0,0); // display

			this->textureWriteState = 2;
		}
		else {
			swapLock->Enter();
			this->textureWriteState = 2;
			swapLock->Leave();
		}
	}

	int Base3DRenderer::GetRenderIndex() 
	{
		if (mode == 0) 
		{
			return 0;
		}
		else 
		{
			int idx = -1;
			swapLock->Enter();
			if (this->textureWriteState == 0 || this->textureWriteState == 1) 
			{
				idx = this->textureIndexWrite;
				this->textureWriteState = 1;
			}
			swapLock->Leave();
			return idx;
		}
	}

	void Base3DRenderer::Run() 
	{
		// NOTE: UpdateWindow MUST be called first!

		swapLock->Enter();
		textureIndexWrite = 0;
		textureWriteState = 0;
		swapLock->Leave();
		while (textureWriteState >= 0) 
		{
			int torender = -1;
			swapLock->Enter();

			if (textureWriteState == 2) 
			{
				torender = textureIndexWrite;
				textureIndexWrite = 2-textureIndexWrite;
				textureWriteState = 0;
			}

			swapLock->Leave();

			if (torender >= 0) 
			{
				CComPtr<IDirect3DSurface9> pBackBuffer;
				device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

				// Clear the backbuffer
				device->SetRenderTarget(0, pBackBuffer);
				device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF0000, 1.0f, 0);

				// Render the frame!
				this->RenderFrame(textures, torender);

				// Draw on the window
				device->Present(0,0,0,0); // display
			}

			Yield();
		}
	}

	void Base3DRenderer::UpdateWindow(int left, int top, int width, int height) 
	{
		swapLock->Enter();
		this->SetWindowSize(left, top, width, height);

		// We create textures and surfaces for each eye that's the same size of the texture we render.
		// This is not done every time - only when we resize the window should this be done.
		if (this->textureWidth != width || this->textureHeight != height) 
		{
			this->textureHeight = height;
			this->textureWidth = width;

			if (textures) 
			{
				// Remove textures first
				for (int i=0; i<4; ++i) 
				{
					textures[i]->Release();
					surfaces[i]->Release();
				}
			}
			else 
			{
				// Allocate texture and surface buffer
				textures = new IDirect3DTexture9*[4];
				surfaces = new IDirect3DSurface9*[4];
			}
			
			// Generate a couple of surfaces
			for (int i = 0; i < 4; ++i) 
			{
				device->CreateTexture(
									width,
									height, 1,
									D3DUSAGE_RENDERTARGET,
									D3DFMT_A32B32G32R32F,
									D3DPOOL_DEFAULT,
									&textures[i],
									nullptr);
				textures[i]->GetSurfaceLevel(0, &surfaces[i]);
			}
	
			// Sets the texture size
			this->UpdateTextureSize(textureWidth, textureHeight);
		}
		swapLock->Leave();
	}
	
	IDirect3DSurface9* Base3DRenderer::GetLeftEye(int renderIndex)
	{
		// Returns the surface for the left eye. We don't check the ptr; it should simply exist. :-)
		return surfaces[renderIndex+0];
	}

	IDirect3DSurface9* Base3DRenderer::GetRightEye(int renderIndex)
	{
		// Returns the surface for the right eye. We don't check the ptr; it should simply exist. :-)
		return surfaces[renderIndex+1];
	}

	Base3DRenderer::~Base3DRenderer() 
	{
		this->swapLock->Enter();
		textureWriteState = -1;
		this->swapLock->Leave();

		Yield();

		delete swapLock;

		// Remove textures and surfaces; the other cleanup is done in the destructor of the library 
		// implementation (e.g. clean up your own code in your destructor please)
		if (textures) 
		{
			// Remove textures first
			for (int i=0; i<4; ++i) 
			{
				textures[i]->Release();
				surfaces[i]->Release();
			}

			// Delete arrays
			delete[] textures;
			delete[] surfaces;
		}
	}
}