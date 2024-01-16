#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include <string>

#include <wincodec.h>
#include <vector>

// this will only call release if an object exists (prevents exceptions calling release on non existant objects)
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

using namespace DirectX; // we will be using the directxmath library


struct Vertex {
	Vertex(float x, float y, float z, float u, float v) : pos(x, y, z), texCoord(u, v) {}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
};

// Handle to the window
HWND hwnd = NULL;

// name of the window (not the title)
LPCTSTR WindowName = L"BzTutsApp";

// title of the window
LPCTSTR WindowTitle = L"Bz Window";

// width and height of the window
int Width = 800;
int Height = 600;

// is window full screen?
bool FullScreen = false;

// we will exit the program when this becomes false
bool Running = true;

// create a window
bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	bool fullscreen);

// main application loop
void mainloop();

// callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);

// direct3d stuff
const int frameBufferCount = 3; // number of buffers we want, 2 for double buffering, 3 for tripple buffering

ID3D12Device* device; // direct3d device

IDXGISwapChain3* swap_chain; // swapchain used to switch between render targets

ID3D12CommandQueue* command_queue; // container for command lists

ID3D12DescriptorHeap* rtvDescriptorHeap; // a descriptor heap to hold resources like the render targets

ID3D12Resource* renderTargets[frameBufferCount]; // number of render targets equal to buffer count

ID3D12CommandAllocator* commandAllocator[frameBufferCount]; // we want enough allocators for each buffer * number of threads (we only have one thread)

ID3D12GraphicsCommandList* commandList; // a command list we can record commands into, then execute them to render the frame

ID3D12Fence* fence[frameBufferCount];    // an object that is locked while our command list is being executed by the gpu. We need as many 
//as we have allocators (more if we want to know when the gpu is finished with an asset)

HANDLE fenceEvent; // a handle to an event when our fence is unlocked by the gpu

UINT64 fenceValue[frameBufferCount]; // this value is incremented each frame. each fence will have its own value
IDXGIFactory4* factory;
int frameIndex; // current rtv we are on
D3D12_STATIC_SAMPLER_DESC sampler;
int rtvDescriptorSize; // size of the rtv descriptor on the device (all front and back buffers will be the same size)
// function declarations

bool InitD3D(); // initializes direct3d 12
bool initialise_swap_chain();
bool initialise_work_submission();
void Update(); // update the game logic

void UpdatePipeline(); // update the direct3d pipeline (update command lists)

void Render(); // execute the command list

void Cleanup(); // release com ojects and clean up memory

void WaitForPreviousFrame(); // wait until gpu is finished with command list
bool build_shaders_and_input_layout();
bool build_pso();
bool build_geometry();
bool build_root_signature();
void build_viewport_scissor_rect();

D3D12_SHADER_BYTECODE pixel_shader_byte_code;
D3D12_SHADER_BYTECODE vertex_shader_byte_code;
std::vector< D3D12_INPUT_ELEMENT_DESC> input_layout;
ID3D12PipelineState* pso; // pso containing a pipeline state

ID3D12RootSignature* rootSignature; // root signature defines data shaders will access

D3D12_VIEWPORT viewport; // area that output from rasterizer will be stretched to.

D3D12_RECT scissorRect; // the area to draw in. pixels outside that area will not be drawn onto

ID3D12Resource* depthStencilBuffer; // This is the memory for our depth buffer. it will also be used for a stencil buffer in a later tutorial
ID3D12DescriptorHeap* dsDescriptorHeap; // This is a heap for our depth/stencil buffer descriptor

ID3DBlob* vertex_shader_data;
ID3DBlob* pixel_shader_data;
// this is the structure of our constant buffer.
struct ConstantBufferPerObject {
	XMFLOAT4X4 wvpMat;
};

// Constant buffers must be 256-byte aligned which has to do with constant reads on the GPU.
// We are only able to read at 256 byte intervals from the start of a resource heap, so we will
// make sure that we add padding between the two constant buffers in the heap (one for cube1 and one for cube2)
// Another way to do this would be to add a float array in the constant buffer structure for padding. In this case
// we would need to add a float padding[50]; after the wvpMat variable. This would align our structure to 256 bytes (4 bytes per float)
// The reason i didn't go with this way, was because there would actually be wasted cpu cycles when memcpy our constant
// buffer data to the gpu virtual address. currently we memcpy the size of our structure, which is 16 bytes here, but if we
// were to add the padding array, we would memcpy 64 bytes if we memcpy the size of our structure, which is 50 wasted bytes
// being copied.
int ConstantBufferPerObjectAlignedSize = (sizeof(ConstantBufferPerObject) + 255) & ~255;

ConstantBufferPerObject cbPerObject; // this is the constant buffer data we will send to the gpu 
// (which will be placed in the resource we created above)

ID3D12Resource* constantBufferUploadHeaps[frameBufferCount]; // this is the memory on the gpu where constant buffers for each frame will be placed

UINT8* cbvGPUAddress[frameBufferCount]; // this is a pointer to each of the constant buffer resource heaps

XMFLOAT4X4 cameraProjMat; // this will store our projection matrix
XMFLOAT4X4 cameraViewMat; // this will store our view matrix

struct Geometry
{
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view()
	{
		D3D12_VERTEX_BUFFER_VIEW view;

		view.BufferLocation = vertex_default->GetGPUVirtualAddress();
		view.StrideInBytes = sizeof(Vertex);
		view.SizeInBytes = vertex_buffer_size;
		return view;
	}

	D3D12_INDEX_BUFFER_VIEW index_buffer_view()
	{
		D3D12_INDEX_BUFFER_VIEW view;

		view.BufferLocation = index_default->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
		view.SizeInBytes = index_buffer_size;
		return view;
	}
	ID3D12Resource* vertex_default; // a default buffer in GPU memory that we will load vertex data for our triangle into
	ID3D12Resource* index_default; // a default buffer in GPU memory that we will load index data for our triangle into

	ID3D12Resource* vertex_upload;
	ID3D12Resource* index_upload; 

	XMFLOAT4X4 world; 
	XMFLOAT4X4 rotation;
	XMFLOAT4 position; 
	int index_buffer_size;
	int index_count;
	int vertex_buffer_size;
};

struct Texture
{
	std::wstring file_name;	
	Microsoft::WRL::ComPtr<ID3D12Resource> texture_default_buffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> texture_upload_buffer = nullptr;
};
int numCubeIndices; // the number of indices to draw the cube

ID3D12Resource* textureBuffer; // the resource heap containing our texture
ID3D12Resource* textureBuffer1; // the resource heap containing our texture
DXGI_SAMPLE_DESC sample_desc;
ID3D12DescriptorHeap* texture_heap;
ID3D12Resource* textureBufferUploadHeap;

void load_texture();
Geometry* cube = new Geometry();
Texture* cube_texture = new Texture();
Texture* cube_normal = new Texture();