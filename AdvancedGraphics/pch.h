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

const int frame_buffer_count = 3;
// we will exit the program when this becomes false
bool Running = true;
// width and height of the window
int Width = 800;
int Height = 600;
// create a window
ID3D12Device* device; // direct3d device
//Constant Buffer Data Per Object
struct ObjectConstantBuffer
{
	XMFLOAT4X4 wvpMat;
};
struct DefaultConstantBuffer
{

};
struct Vertex {
	Vertex(float x, float y, float z, float u, float v) : pos(x, y, z), texCoord(u, v) {}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
};
struct default_buffer
{
	ID3D12Resource* create_default_buffer()
	{
		//If we are creating constant buffer upload buffer, elements need to be multiples of
		//256 bytes. This is due to hardware can only view constant data at elements at this size with offset.

		ID3D12Resource* buffer = nullptr;;

		// Create the actual default buffer resource.
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byte_size),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&buffer));

		return buffer;
	}
	int byte_size;
	ID3D12Resource* default_buffer;
};
struct Geometry
{
	std::wstring name;
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
struct Shader
{
	Shader(LPCWSTR file_name, LPCSTR entry, LPCSTR target)
	{

		HRESULT hr;
		hr = D3DCompileFromFile(file_name,
			nullptr,
			nullptr,
			entry,
			target,
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			&shader_data,
			&error);
		if (FAILED(hr))
		{
			OutputDebugStringA((char*)error->GetBufferPointer());
		}
	}
	ID3DBlob* error;
	LPCWSTR file_name;
	LPCSTR entry_point;
	LPCSTR target;
	bool failed;
	ID3DBlob* shader_data;
	D3D12_SHADER_BYTECODE shader_byte_code()
	{
		D3D12_SHADER_BYTECODE byte_code;
		byte_code = {};
		byte_code.BytecodeLength = shader_data->GetBufferSize();
		byte_code.pShaderBytecode = shader_data->GetBufferPointer();

		return byte_code;
	};

};
struct upload_buffer
{
	void create_upload_buffer(int byte_size , int element_count)
	{
		element_byte_size = byte_size;
		HRESULT hr;
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&upload_buffer));


		if (FAILED(hr))
		{
			Running = false;
		}
		hr = upload_buffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped_data));

		upload_buffer->SetName(L"Constant Buffer Upload Resource Heap");
	

	}
	void copy_data(int element , ObjectConstantBuffer data)
	{
		memcpy(&mapped_data[element * element_byte_size], &data, sizeof(ObjectConstantBuffer));

	}
	BYTE* mapped_data = nullptr;

	ID3D12Resource* upload_buffer;
	int element_byte_size;

};
struct FrameResource
{


	FrameResource(int object_count)
	{
		constant_buffer_object = new upload_buffer();
		constant_buffer_default = new upload_buffer();

		constant_buffer_per_object_byte_size = (sizeof(ObjectConstantBuffer) + 255) & ~255;
		//constant_buffer_default_byte_size = (sizeof(DefaultConstantBuffer) + 255) & ~255;

		constant_buffer_object->create_upload_buffer(constant_buffer_per_object_byte_size, object_count);
		//constant_buffer_default->create_upload_buffer(constant_buffer_default_byte_size, 1);
	}

	int constant_buffer_per_object_byte_size;
	int constant_buffer_default_byte_size;
	ObjectConstantBuffer cbPerObject; 
	upload_buffer* constant_buffer_object;
	upload_buffer* constant_buffer_default;
	UINT8* cb_gpu_object_address;


};
struct depth
{


	D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view()
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC view = {};

		view.Format = DXGI_FORMAT_D32_FLOAT;
		view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		view.Flags = D3D12_DSV_FLAG_NONE;
		
		return view;
	}

	D3D12_CLEAR_VALUE clear_value()
	{
		D3D12_CLEAR_VALUE value = {};
		value.Format = DXGI_FORMAT_D32_FLOAT;
		value.DepthStencil.Depth = 1.0f;
		value.DepthStencil.Stencil = 0;

		return value;
	}

	ID3D12DescriptorHeap* depth_heap;
	ID3D12Resource* depth_stencil_data;
};
struct rtv
{
	int rtv_descriptor_size;
	ID3D12DescriptorHeap* rtv_heap;
	ID3D12Resource* render_targets[frame_buffer_count];
};

DXGI_MODE_DESC back_buffer_desc()
{
	DXGI_MODE_DESC desc = {};
	desc.Width = Width;
	desc.Height = Height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	return desc;
}



//For CPU - GPU synchronization
ID3D12Fence* fence[frame_buffer_count];
HANDLE fence_event;
UINT64 fence_value[frame_buffer_count];
// Handle to the window
HWND hwnd = NULL;

LPCTSTR name = L"Advance Graphics";



// is window full screen?
bool full_screen = false;


bool initialise_window(HINSTANCE hInstance,int ShowWnd,bool fullscreen);

// main application loop
void mainloop();

// callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);



ID3D12CommandAllocator* command_allocator[frame_buffer_count];

IDXGISwapChain3* swap_chain; // swapchain used to switch between render targets

ID3D12CommandQueue* command_queue; 
ID3D12GraphicsCommandList* command_list;

std::vector<FrameResource*> frame_resources;
IDXGIFactory4* factory;
int frame_index; // current rtv we are on
D3D12_STATIC_SAMPLER_DESC sampler;
// function declarations

bool init_d3d(); // initializes direct3d 12
bool init_dxgi();
bool initialise_swap_chain();
bool initialise_work_submission();
void Update(); // update the game logic

void UpdatePipeline(); // update the direct3d pipeline (update command lists)

void Render(); // execute the command list

void Cleanup(); // release com ojects and clean up memory

void WaitForPreviousFrame(); 
bool build_shaders_and_input_layout();
bool build_pso();
bool build_geometry();
bool build_constant_views();

bool build_descriptor_heaps();
bool build_frame_resources();


//Optional Geometry Objects we can draw
bool build_cube();
bool build_grid();

bool build_root_signature();
void build_viewport_scissor_rect();

IDXGIAdapter1* adapter; // adapters are the graphics card (this includes the embedded graphics on the motherboard)
int adapter_index = 0; // we'll start looking for directx 12  compatible graphics devices starting at index 0
bool adapter_found = false; // set this to true when a good one was found

std::vector< D3D12_INPUT_ELEMENT_DESC> input_layout;
ID3D12PipelineState* pso; // pso containing a pipeline state

ID3D12RootSignature* rootSignature; // root signature defines data shaders will access

D3D12_VIEWPORT viewport; // area that output from rasterizer will be stretched to.

D3D12_RECT scissorRect; // the area to draw in. pixels outside that area will not be drawn onto


//Descriptor Heaps - Stores data outside of PSO (SRVs, RTVs, DSVs ect..)

depth* main_depth;
rtv* main_rtv;
ID3D12DescriptorHeap* srv_heap;
ID3D12DescriptorHeap* cbv_heap;
FrameResource* frame;


//Buffers - Stores memeory/data

Texture* cube_texture = new Texture();
Texture* cube_normal = new Texture();


int cbv_offset = 0;
XMFLOAT4X4 cameraProjMat;
XMFLOAT4X4 cameraViewMat;


int cbv_srv_uav_descriptor_size;
DXGI_SAMPLE_DESC sample_desc;


void load_texture();


Shader* shader_vertex;
Shader* shader_pixel;

std::vector<Geometry*> objects;