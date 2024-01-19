#include "pch.h"

#include "camera.h"


Camera* g_pCamera;


int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd)

{
	// create the window
	if (!initialise_window(hInstance, nShowCmd, full_screen))
	{
		MessageBox(0, L"Window Initialization - Failed",
			L"Error", MB_OK);
		return 1;
	}

	// initialize direct3d
	if (!init_d3d())
	{
		MessageBox(0, L"Failed to initialize direct3d 12",
			L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// start the main loop
	mainloop();

	// we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame();

	// close the fence event
	CloseHandle(fence_event);

	// clean up everything
	Cleanup();

	return 0;
}

bool initialise_window(HINSTANCE hInstance,
	int ShowWnd,
	bool fullscreen)

{
	if (fullscreen)
	{
		HMONITOR hmon = MonitorFromWindow(hwnd,
			MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		Width = mi.rcMonitor.right - mi.rcMonitor.left;
		Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = name;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(NULL,
		name,
		name,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		Width, Height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop() {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));


	while (Running)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// run game code
			Update(); // update the game logic
			Render(); // execute the command queue (rendering the scene is the result of the gpu executing the command lists)
		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)

{
	float movement = 0.1f;
	static bool mouseDown = false;

	switch (msg)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 27:
			Running = false;
			PostQuitMessage(0);
			break;
		case 'W':
			g_pCamera->MoveForward(movement);  // Adjust distance as needed
			break;
		case 'A':
			g_pCamera->StrafeLeft(movement);  // Adjust distance as needed
			break;
		case 'S':
			g_pCamera->MoveBackward(movement);  // Adjust distance as needed
			break;
		case 'D':
			g_pCamera->StrafeRight(movement);  // Adjust distance as needed
			break;
		}
		break;
	case WM_LBUTTONDOWN:
		mouseDown = true;
		break;
	case WM_LBUTTONUP:
		mouseDown = false;
		break;
	case WM_MOUSEMOVE:
	{
		if (!mouseDown)
		{
			break;
		}
		// Get the dimensions of the window
		RECT rect;
		GetClientRect(hwnd, &rect);

		// Calculate the center position of the window
		POINT windowCenter;
		windowCenter.x = (rect.right - rect.left) / 2;
		windowCenter.y = (rect.bottom - rect.top) / 2;

		// Convert the client area point to screen coordinates
		ClientToScreen(hwnd, &windowCenter);

		// Get the current cursor position
		POINTS mousePos = MAKEPOINTS(lParam);
		POINT cursorPos = { mousePos.x, mousePos.y };
		ClientToScreen(hwnd, &cursorPos);

		// Calculate the delta from the window center
		POINT delta;
		delta.x = cursorPos.x - windowCenter.x;
		delta.y = cursorPos.y - windowCenter.y;

		// Update the camera with the delta
		// (You may need to convert POINT to POINTS or use the deltas as is)
		g_pCamera->UpdateLookAt({ static_cast<short>(delta.x), static_cast<short>(delta.y) });

		// Recenter the cursor
		SetCursorPos(windowCenter.x, windowCenter.y);
	}
	break;

	case WM_DESTROY: // x button on top right corner of window was pressed
		Running = false;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,
		msg,
		wParam,
		lParam);
}

BYTE* imageData;

bool init_d3d()
{
	HRESULT hr;

	init_dxgi();
	// Create the device
	hr = D3D12CreateDevice(
		adapter,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device)
	);
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	initialise_work_submission();
	initialise_swap_chain();

	frame_index = swap_chain->GetCurrentBackBufferIndex();

	build_root_signature();
	build_shaders_and_input_layout();

	build_geometry();
	build_descriptor_heaps();

	load_texture();
	build_frame_resources();
	build_constant_views();

	build_pso();
	// load the image, create a texture resource and descriptor heap

	
	// Now we execute the command list to upload the initial assets (triangle data)
	command_list->Close();
	ID3D12CommandList* ppCommandLists[] = { command_list };
	command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
	fence_value[frame_index]++;
	hr = command_queue->Signal(fence[frame_index], fence_value[frame_index]);
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}

	// we are done with image data now that we've uploaded it to the gpu, so free it up
	delete imageData;



	build_viewport_scissor_rect();

	// build projection and view matrix
	XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(45.0f * (3.14f / 180.0f), (float)Width / (float)Height, 0.1f, 1000.0f);
	XMStoreFloat4x4(&cameraProjMat, tmpMat);

	g_pCamera = new Camera(XMFLOAT3(0.0f, 0, -3), XMFLOAT3(0, 0, 1), XMFLOAT3(0.0f, 1.0f, 0.0f));

	// set starting cubes position
	// first cube
	objects.at(0)->position = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // set cube 1's position
	XMVECTOR posVec = XMLoadFloat4(&objects.at(0)->position); // create xmvector for cube1's position

	tmpMat = XMMatrixTranslationFromVector(posVec); // create translation matrix from cube1's position vector
	XMStoreFloat4x4(&objects.at(0)->rotation, XMMatrixIdentity()); // initialize cube1's rotation matrix to identity matrix
	XMStoreFloat4x4(&objects.at(0)->world, tmpMat); // store cube1's world matrix


	return true;
}

bool init_dxgi()
{
	HRESULT hr;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr))
	{
		return false;
	}


	// find first hardware gpu that supports d3d 12
	while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// we dont want a software device
			continue;
		}

		// we want a device that is compatible with direct3d 12 (feature level 11 or higher)
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			adapter_found = true;
			break;
		}

		adapter_index++;
	}

	if (!adapter_found)
	{
		Running = false;
		return false;
	}
	return true;
}

bool initialise_swap_chain()
{
	HRESULT hr;
	// -- Create the Swap Chain (double/tripple buffering) -- //


	sample_desc = {};
	sample_desc.Count = 1; 


	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = frame_buffer_count; 
	desc.BufferDesc = back_buffer_desc(); 
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; 
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; 
	desc.OutputWindow = hwnd; 
	desc.SampleDesc = sample_desc; 
	desc.Windowed = !full_screen;
	IDXGISwapChain* temp;

	factory->CreateSwapChain(
		command_queue, // the queue will be flushed once the swap chain is created
		&desc, // give it the swap chain description we created above
		&temp // store the created swap chain in a temp IDXGISwapChain interface
	);

	//Cast swap chain, converts IDXGISwapChain to IDXGISwapChain3
	swap_chain = static_cast<IDXGISwapChain3*>(temp);

	return true;
}

bool initialise_work_submission()
{

	HRESULT hr;
	// -- Create a direct command queue -- //

	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct means the gpu can directly execute this command queue

	hr = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)); // create the command queue
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	// -- Create the Command Allocators -- //

	for (int i = 0; i < frame_buffer_count; i++)
	{
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator[i]));
		if (FAILED(hr))
		{
			Running = false;
			return false;
		}
	}

	// -- Create a Command List -- //

	// create the command list with the first allocator
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator[frame_index], NULL, IID_PPV_ARGS(&command_list));
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
}

void Update()
{
	//Update Game logic



	auto object_cb = frame_resources.at(frame_index)->constant_buffer_object;

	for (int i = 0; i < objects.size(); i++)
	{

		// create rotation matrices
		XMMATRIX rotation_x = XMMatrixRotationX(0.0001f);
		XMMATRIX rotation_y = XMMatrixRotationY(0.0002f);
		XMMATRIX rotation_z = XMMatrixRotationZ(0.0003f);

		ObjectConstantBuffer data;
		//Concatenate rotation matrix for cube
		XMMATRIX rotation_matrix = XMLoadFloat4x4(&objects.at(i)->rotation) * rotation_x * rotation_y * rotation_z;
		XMStoreFloat4x4(&objects.at(i)->rotation, rotation_matrix);

		// create translation matrix for cube 1 from cube 1's position vector
		//Translation matrix, loaded from positional vector
		XMMATRIX translation_matrix = XMMatrixTranslationFromVector(XMLoadFloat4(&objects.at(0)->position));
		// create cube1's world matrix by first rotating the cube, then positioning the rotated cube
		XMMATRIX worldMat = rotation_matrix * translation_matrix;
		// store cube1's world matrix
		XMStoreFloat4x4(&objects.at(i)->world, worldMat);

		// update constant buffer for cube1 (for each object, we need a constant buffer view)
		// create the wvp matrix and store in constant buffer

		//Update Constant Buffer for cub
		XMMATRIX view = g_pCamera->GetViewMatrix(); // load view matrix
		XMMATRIX projection = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
		XMMATRIX world_view_projection = XMLoadFloat4x4(&objects.at(i)->world) * view * projection; // create wvp matrix
		XMMATRIX transposed = XMMatrixTranspose(world_view_projection); // must transpose wvp matrix for the gpu

		XMStoreFloat4x4(&data.wvpMat, transposed); // store transposed wvp matrix in constant buffer
		// copy our ConstantBuffer instance to the mapped constant buffer resource
		object_cb->copy_data(0, data);
	}
}

void UpdatePipeline()
{
	HRESULT hr;

	WaitForPreviousFrame();

	hr = command_allocator[frame_index]->Reset();

	if (FAILED(hr))
	{
		Running = false;
	}
	hr = command_list->Reset(command_allocator[frame_index], pso);
	if (FAILED(hr))
	{
		Running = false;
	}

	// here we start recording commands into the commandList (which all the commands will be stored in the commandAllocator)

	// transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(main_rtv->render_targets[frame_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// here we again get the handle to our current render target view so we can set it as the render target in the output merger stage of the pipeline
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(main_rtv->rtv_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, main_rtv->rtv_descriptor_size);

	// get a handle to the depth/stencil buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(main_depth->depth_heap->GetCPUDescriptorHandleForHeapStart());

	// set the render target for the output merger stage (the output of the pipeline)
	command_list->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Clear the render target by using the ClearRenderTargetView command
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// clear the depth/stencil buffer
	command_list->ClearDepthStencilView(main_depth->depth_heap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// set root signature
	command_list->SetGraphicsRootSignature(rootSignature); // set the root signature

	// set the descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { srv_heap };
	command_list->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set the descriptor table to the descriptor heap (parameter 1, as constant buffer root descriptor is parameter index 0)
	command_list->SetGraphicsRootDescriptorTable(1, srv_heap->GetGPUDescriptorHandleForHeapStart());

	command_list->RSSetViewports(1, &viewport); // set the viewports
	command_list->RSSetScissorRects(1, &scissorRect); // set the scissor rects
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology
	command_list->IASetVertexBuffers(0, 1, &objects.at(0)->vertex_buffer_view()); // set the vertex buffer (using the vertex buffer view)
	command_list->IASetIndexBuffer(&objects.at(0)->index_buffer_view());

	// first cube

	// set cube1's constant buffer
	command_list->SetGraphicsRootConstantBufferView(0, frame_resources.at(frame_index)->constant_buffer_object->upload_buffer->GetGPUVirtualAddress());

	// draw first cube
	command_list->DrawIndexedInstanced(objects.at(0)->index_count, 1, 0, 0, 0);

	

	// transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
	// warning if present is called on the render target when it's not in the present state
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(main_rtv->render_targets[frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	hr = command_list->Close();
	if (FAILED(hr))
	{
		Running = false;
	}
}
void Render()
{
	HRESULT hr;

	UpdatePipeline(); // update the pipeline by sending commands to the command_queue

	// create an array of command lists (only one command list here)
	ID3D12CommandList* ppCommandLists[] = { command_list };

	// execute the array of command lists
	command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// this command goes in at the end of our command queue. we will know when our command queue 
	// has finished because the fence value will be set to "fenceValue" from the GPU since the command
	// queue is being executed on the GPU
	hr = command_queue->Signal(fence[frame_index], fence_value[frame_index]);
	if (FAILED(hr))
	{
		Running = false;
	}

	// present the current backbuffer
	hr = swap_chain->Present(0, 0);
	if (FAILED(hr))
	{
		Running = false;
	}
}

void Cleanup()
{
	// wait for the gpu to finish all frames
	for (int i = 0; i < frame_buffer_count; ++i)
	{
		frame_index = i;
		WaitForPreviousFrame();
	}

	// get swapchain out of full screen before exiting
	BOOL fs = false;
	if (swap_chain->GetFullscreenState(&fs, NULL))
		swap_chain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(swap_chain);
	SAFE_RELEASE(command_queue);
	SAFE_RELEASE(main_rtv->rtv_heap);
	SAFE_RELEASE(command_list);

	for (int i = 0; i < frame_buffer_count; ++i)
	{
		SAFE_RELEASE(main_rtv->render_targets[i]);
		SAFE_RELEASE(command_allocator[i]);
		SAFE_RELEASE(fence[i]);
	};

	SAFE_RELEASE(pso);
	SAFE_RELEASE(rootSignature);
	//SAFE_RELEASE(vertexBuffer);
	//SAFE_RELEASE(indexBuffer);

	SAFE_RELEASE(main_depth->depth_stencil_data);
	SAFE_RELEASE(main_depth->depth_heap);

	for (int i = 0; i < frame_buffer_count; ++i)
	{
		//SAFE_RELEASE(constant_buffer_upload_heaps[i]);
	};
}

void WaitForPreviousFrame()
{
	HRESULT hr;

	// swap the current rtv buffer index so we draw on the correct buffer
	frame_index = swap_chain->GetCurrentBackBufferIndex();

	// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
	// the command queue since it has not reached the "command_queue->Signal(fence, fenceValue)" command
	if (fence[frame_index]->GetCompletedValue() < fence_value[frame_index])
	{
		// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
		hr = fence[frame_index]->SetEventOnCompletion(fence_value[frame_index], fence_event);
		if (FAILED(hr))
		{
			Running = false;
		}

		// We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
		// has reached "fenceValue", we know the command queue has finished executing
		WaitForSingleObject(fence_event, INFINITE);
	}

	// increment fenceValue for next frame
	fence_value[frame_index]++;
}

bool build_shaders_and_input_layout()
{
	shader_vertex = new Shader(L"VertexShader.hlsl" , "main" , "vs_5_0");

	if (shader_vertex->failed == false)
	{
		OutputDebugStringA((char*)shader_vertex->error->GetBufferPointer());
		Running = false;
		return false;
	}

	shader_pixel = new Shader(L"PixelShader.hlsl", "main", "ps_5_0");

	if (shader_pixel->failed == false)
	{
		OutputDebugStringA((char*)shader_vertex->error->GetBufferPointer());
		Running = false;
		return false;
	}



	// create input layout

	// The input layout is used by the Input Assembler so that it knows
	// how to read the vertex data bound to it.

	input_layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	return true;

}

bool build_pso()
{
	HRESULT hr;

	D3D12_INPUT_LAYOUT_DESC input_layout_desc = {};
	input_layout_desc.NumElements = input_layout.size();
	input_layout_desc.pInputElementDescs = input_layout.data();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.InputLayout = input_layout_desc; 
	pso_desc.pRootSignature = rootSignature; 
	pso_desc.VS = shader_vertex->shader_byte_code(); 
	pso_desc.PS = shader_pixel->shader_byte_code(); 
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; 
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; 
	pso_desc.SampleDesc = sample_desc; 
	pso_desc.SampleMask = 0xffffffff; 
	pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); 
	pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pso_desc.NumRenderTargets = 3; 
	pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); 
	// create the pso
	hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
}

bool build_geometry()
{
	HRESULT hr;

	build_cube();
	return true;
}


bool build_constant_views()
{
	HRESULT hr;
	cbv_srv_uav_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	int object_cb_byte_size = frame_resources.at(0)->constant_buffer_per_object_byte_size;
	int object_count = objects.size();
	for (int frame = 0; frame < frame_buffer_count; ++frame)
	{
		auto object_cb = frame_resources.at(frame)->constant_buffer_object->upload_buffer;

		for (int j = 0; j < object_count; j++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS address = object_cb->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			address += j * object_cb_byte_size;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frame * object_count + j;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbv_heap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, cbv_srv_uav_descriptor_size);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = address;

			cbvDesc.SizeInBytes = object_cb_byte_size;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	return true;
}


bool build_descriptor_heaps()
{
	HRESULT hr;
	main_depth = new depth();
	main_rtv = new rtv();

	//Build DSV, SRV and RTV heaps
	D3D12_DESCRIPTOR_HEAP_DESC dsv_desc = {};
	dsv_desc.NumDescriptors = 1;
	dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&main_depth->depth_heap));

	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	
	D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
	rtv_desc.NumDescriptors = frame_buffer_count;
	rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&main_rtv->rtv_heap));

	if (FAILED(hr))
	{
		Running = false;
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC srv_desc = {};
	srv_desc.NumDescriptors = 1;
	srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hr = device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&srv_heap));
	if (FAILED(hr))
	{
		Running = false;
	}



	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT num_descriptors = (objects.size() * frame_buffer_count);

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	cbv_offset = objects.size()* frame_buffer_count;

	D3D12_DESCRIPTOR_HEAP_DESC cbv_desc;
	cbv_desc.NumDescriptors = num_descriptors;
	cbv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbv_desc.NodeMask = 0;
	
	device->CreateDescriptorHeap(&cbv_desc, IID_PPV_ARGS(&cbv_heap));
	
	srv_heap->SetName(L"SRV Resource Heap");;
	main_depth->depth_heap->SetName(L"Depth/Stencil Resource Heap");
	main_rtv->rtv_heap->SetName(L"RTV Resource Heap");
	cbv_heap->SetName(L"RTV Resource Heap");


	//Build RTV & DSV buffers 
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, Width, Height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&main_depth->clear_value(),
		IID_PPV_ARGS(&main_depth->depth_stencil_data)
	);
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}


	//Create All Views
	device->CreateDepthStencilView(main_depth->depth_stencil_data, &main_depth->depth_stencil_view(), main_depth->depth_heap->GetCPUDescriptorHandleForHeapStart());



	main_rtv->rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(main_rtv->rtv_heap->GetCPUDescriptorHandleForHeapStart());

	// Create a RTV for each buffer (double buffering is two buffers, tripple buffering is 3).
	for (int i = 0; i < frame_buffer_count; i++)
	{
		// first we get the n'th buffer in the swap chain and store it in the n'th
		// position of our ID3D12Resource array
		hr = swap_chain->GetBuffer(i, IID_PPV_ARGS(&main_rtv->render_targets[i]));
		if (FAILED(hr))
		{
			Running = false;
			return false;
		}

		// the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
		device->CreateRenderTargetView(main_rtv->render_targets[i], nullptr, rtv_handle);

		// we increment the rtv handle by the rtv descriptor size we got above
		rtv_handle.Offset(1, main_rtv->rtv_descriptor_size);
	}


	return true;
}

bool build_frame_resources()
{
	HRESULT hr;
	//BUild GPU - CPU synchronization objects
	frame = new FrameResource(objects.size());

	// -- Create a Fence & Fence Event -- //

// create the fences
	for (int i = 0; i < frame_buffer_count; i++)
	{
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
		if (FAILED(hr))
		{
			Running = false;
			return false;
		}
	   fence_value[i] = 0; // set the initial fence value to 0
	}

	// create a handle to a fence event
	 fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
	{
		Running = false;
		return false;
	}

	for (int i = 0; i < frame_buffer_count; i++)
	{
			FrameResource* resource = new FrameResource(objects.size());
	frame_resources.push_back(resource);
	}

	return true;
}

bool build_cube()
{
	HRESULT hr;
	Geometry* cube = new Geometry();
	cube->name = L"cube 1";
	std::vector<Vertex> vertices = {
		// front face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 0.0f },
		{  0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// right side face
		{  0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{  0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 0.0f, 0.0f },

		// left side face
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// back face
		{  0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{  0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 1.0f, 0.0f },

		// top face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{  0.5f,  0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },

		// bottom face
		{  0.5f, -0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{  0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 0.0f },
	};

	cube->vertex_buffer_size = vertices.size() * sizeof(Vertex);

	// create default heap
	// default heap is memory on the GPU. Only the GPU has access to this memory
	// To get data into this heap, we will have to upload the data using
	// an upload heap
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(cube->vertex_buffer_size), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
		// from the upload heap to this heap
		nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&cube->vertex_default));
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	cube->vertex_default->SetName(L"Vertex Buffer Resource Heap");

	// create upload heap
	// upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
	// We will upload the vertex buffer using this heap to the default heap

	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(cube->vertex_buffer_size), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&cube->vertex_upload));
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	cube->vertex_upload->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertex_data = {};
	vertex_data.pData = vertices.data(); // pointer to our vertex array
	vertex_data.RowPitch = cube->vertex_buffer_size; // size of all our triangle vertex data
	vertex_data.SlicePitch = cube->vertex_buffer_size; // also the size of our triangle vertex data

	// we are now creating a command with the command list to copy the data from
	// the upload heap to the default heap
	UpdateSubresources(command_list, cube->vertex_default, cube->vertex_upload, 0, 0, 1, &vertex_data);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cube->vertex_default, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Create index buffer

	// a quad (2 triangles)
	std::vector<int> indices = {
		// ffront face
		0, 1, 2, // first triangle
		0, 3, 1, // second triangle

		// left face
		4, 5, 6, // first triangle
		4, 7, 5, // second triangle

		// right face
		8, 9, 10, // first triangle
		8, 11, 9, // second triangle

		// back face
		12, 13, 14, // first triangle
		12, 15, 13, // second triangle

		// top face
		16, 17, 18, // first triangle
		16, 19, 17, // second triangle

		// bottom face
		20, 21, 22, // first triangle
		20, 23, 21, // second triangle
	};

	cube->index_buffer_size = indices.size() * sizeof(int);

	cube->index_count = indices.size() ;

	// create default heap to hold index buffer
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(cube->index_buffer_size), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
		nullptr, // optimized clear value must be null for this type of resource
		IID_PPV_ARGS(&cube->index_default));
 	if (FAILED(hr))
	{
		Running = false;
		return false;
	}

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	cube->index_default->SetName(L"Index Buffer Resource Heap");

	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(cube->index_buffer_size), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&cube->index_upload));
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	cube->index_upload->SetName(L"Index Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA index_data = {};
	index_data.pData = indices.data(); // pointer to our index array
	index_data.RowPitch = cube->index_buffer_size; // size of all our index buffer
	index_data.SlicePitch = cube->index_buffer_size; // also the size of our index buffer

	// we are now creating a command with the command list to copy the data from
	// the upload heap to the default heap
	UpdateSubresources(command_list, cube->index_default, cube->index_upload, 0, 0, 1, &index_data);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cube->index_default, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	objects.push_back(cube);
}

bool build_grid()
{
	HRESULT hr;
	//Geometry* grid = new Geometry();
	//int vertex_count = 40.0f * 60.0f;
	//int face_count = (40.0f - 1) * (60.0f - 1) * 2;

	//float half_width = 0.5f * 20.0f;
	//float half_depth = 0.5f * 30.0f;

	//float dx = 30.0f / (40.0f - 1);
	//float dz = 30.0f / (60.0f - 1);
	//float du = 1.0f / (40.0f - 1);
	//float dv = 1.0f / (60l0f - 1);
	//std::vector<Vertex> vertices;
	//vertices.resize(vertex_count);

	//for (int i = 0; i < 60.0f; ++i)
	//{
	//	float z = half_depth - i * dz;
	//	for (int j = 0; j < 40.0f; ++j)
	//	{
	//		float x = -half_width + j * dx;

	//		vertices[i * 40.0f + j].pos = XMFLOAT3(x, 0.0f, z);

	//		// Stretch texture over grid.
	//		vertices[i * 40.0f + j].texCoord.x = j * du;
	//		vertices[i * 40.0f + j].texCoord.y = i * dv;
	//	}
	//}
	//cube->vertex_buffer_size = sizeof(vertices);

	//// create default heap
	//// default heap is memory on the GPU. Only the GPU has access to this memory
	//// To get data into this heap, we will have to upload the data using
	//// an upload heap
	//hr = device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
	//	D3D12_HEAP_FLAG_NONE, // no flags
	//	&CD3DX12_RESOURCE_DESC::Buffer(cube->vertex_buffer_size), // resource description for a buffer
	//	D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
	//	// from the upload heap to this heap
	//	nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
	//	IID_PPV_ARGS(&cube->vertex_default));
	//if (FAILED(hr))
	//{
	//	Running = false;
	//	return false;
	//}

	//// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	//cube->vertex_default->SetName(L"Vertex Buffer Resource Heap");

	//// create upload heap
	//// upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
	//// We will upload the vertex buffer using this heap to the default heap

	//hr = device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
	//	D3D12_HEAP_FLAG_NONE, // no flags
	//	&CD3DX12_RESOURCE_DESC::Buffer(cube->vertex_buffer_size), // resource description for a buffer
	//	D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
	//	nullptr,
	//	IID_PPV_ARGS(&cube->vertex_upload));
	//if (FAILED(hr))
	//{
	//	Running = false;
	//	return false;
	//}
	//cube->vertex_upload->SetName(L"Vertex Buffer Upload Resource Heap");

	//// store vertex buffer in upload heap
	//D3D12_SUBRESOURCE_DATA vertex_data = {};
	//vertex_data.pData = reinterpret_cast<BYTE*>(vertices); // pointer to our vertex array
	//vertex_data.RowPitch = cube->vertex_buffer_size; // size of all our triangle vertex data
	//vertex_data.SlicePitch = cube->vertex_buffer_size; // also the size of our triangle vertex data

	//// we are now creating a command with the command list to copy the data from
	//// the upload heap to the default heap
	//UpdateSubresources(commandList, cube->vertex_default, cube->vertex_upload, 0, 0, 1, &vertex_data);

	//// transition the vertex buffer data from copy destination state to vertex buffer state
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cube->vertex_default, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	//std::vector<int> indicies;
	//indicies.resize(face_count * 3);
	//// Iterate over each quad and compute indices.
	//int k = 0;
	//for (int i = 0; i < 60.0f - 1; ++i)
	//{
	//	for (int j = 0; j < 40.0f - 1; ++j)
	//	{
	//		indicies[k] = i * 40.0f + j;
	//		indicies[k + 1] = i * 40.0f + j + 1;
	//		indicies[k + 2] = (i + 1) * 40.0f + j;

	//		indicies[k + 3] = (i + 1) * 40.0f + j;
	//		indicies[k + 4] = i * 40.0f + j + 1;
	//		indicies[k + 5] = (i + 1) * 40.0f + j + 1;

	//		k += 6; // next quad
	//	}
	//}
	//grid->index_buffer_size = sizeof(indices);

	//grid->index_count = sizeof(indices) / sizeof(int);

	//// create default heap to hold index buffer
	//hr = device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
	//	D3D12_HEAP_FLAG_NONE, // no flags
	//	&CD3DX12_RESOURCE_DESC::Buffer(grid->index_buffer_size), // resource description for a buffer
	//	D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
	//	nullptr, // optimized clear value must be null for this type of resource
	//	IID_PPV_ARGS(&grid->index_default));
	//if (FAILED(hr))
	//{
	//	Running = false;
	//	return false;
	//}

	//// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	//cube->index_default->SetName(L"Index Buffer Resource Heap");

	//hr = device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
	//	D3D12_HEAP_FLAG_NONE, // no flags
	//	&CD3DX12_RESOURCE_DESC::Buffer(grid->index_buffer_size), // resource description for a buffer
	//	D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
	//	nullptr,
	//	IID_PPV_ARGS(&cube->index_upload));
	//if (FAILED(hr))
	//{
	//	Running = false;
	//	return false;
	//}
	//cube->index_upload->SetName(L"Index Buffer Upload Resource Heap");

	//// store vertex buffer in upload heap
	//D3D12_SUBRESOURCE_DATA index_data = {};
	//index_data.pData = reinterpret_cast<BYTE*>(indices); // pointer to our index array
	//index_data.RowPitch = grid->index_buffer_size; // size of all our index buffer
	//index_data.SlicePitch = grid->index_buffer_size; // also the size of our index buffer

	//// we are now creating a command with the command list to copy the data from
	//// the upload heap to the default heap
	//UpdateSubresources(commandList, grid->index_default, grid->index_upload, 0, 0, 1, &index_data);

	//// transition the vertex buffer data from copy destination state to vertex buffer state
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(grid->index_default, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	//objects.push_back(grid);

	return true;
}

bool build_root_signature()
{
	HRESULT hr;
	//Root Signature - Links resources the shader require, essentially a list of parameters/objects exposed to shaders.
	//We can store - 
	//Constants , Tables and Descriptors



	// create a descriptor range (descriptor table) and fill it out
	// this is a range of descriptors inside a descriptor heap
	D3D12_DESCRIPTOR_RANGE  descriptorTableRanges[1]; // only one range right now
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // this is a range of shader resource views (descriptors)
	descriptorTableRanges[0].NumDescriptors = 1; // we only have one texture right now, so the range is only 1
	descriptorTableRanges[0].BaseShaderRegister = 0; // start index of the shader registers in the range
	descriptorTableRanges[0].RegisterSpace = 0; // space 0. can usually be zero
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

	// create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); // we only have one range
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0]; // the pointer to the beginning of our ranges array
	// create a root descriptor, which explains where to find the data for this root parameter
	D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
	rootCBVDescriptor.RegisterSpace = 0;
	rootCBVDescriptor.ShaderRegister = 0;
	// create a root parameter for the root descriptor and fill it out
	D3D12_ROOT_PARAMETER  rootParameters[2]; // only one parameter right now
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
	rootParameters[0].Descriptor = rootCBVDescriptor; // this is the root descriptor for this root parameter
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // our pixel shader will be the only shader accessing this parameter for now

	// fill out the parameter for our descriptor table. Remember it's a good idea to sort parameters by frequency of change. Our constant
	// buffer will be changed multiple times per frame, while our descriptor table will not be changed at all (in this tutorial)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
	rootParameters[1].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now

	// create a static sampler
	sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), // we have 2 root parameters
		rootParameters, // a pointer to the beginning of our root parameters array
		1, // we have one static sampler
		&sampler, // a pointer to our static sampler (array)
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // we can deny shader stages here for better performance
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

	ID3DBlob* errorBuff; // a buffer holding the error data if any
	ID3DBlob* signature;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errorBuff);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBuff->GetBufferPointer());
		return false;
	}

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr))
	{
		return false;
	}
}

void build_viewport_scissor_rect()
{
	// Fill out the Viewport
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)Width;
	viewport.Height = (float)Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Fill out a scissor rect
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = Width;
	scissorRect.bottom = Height;
}

void load_texture()
{
	HRESULT hr;
	cube_texture->file_name = L"Textures/bricks.dds";
	hr = DirectX::CreateDDSTextureFromFile12(device, command_list, cube_texture->file_name.c_str(), cube_texture->texture_default_buffer, cube_texture->texture_upload_buffer);

	cube_normal->file_name = L"Textures/normal.dds";
	hr = DirectX::CreateDDSTextureFromFile12(device, command_list, cube_texture->file_name.c_str(), cube_texture->texture_default_buffer, cube_texture->texture_upload_buffer);

	// now we create a shader resource view (descriptor that points to the texture and describes it)
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = cube_texture->texture_default_buffer->GetDesc().Format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(cube_texture->texture_default_buffer.Get(), &desc, srv_heap->GetCPUDescriptorHandleForHeapStart());

}

