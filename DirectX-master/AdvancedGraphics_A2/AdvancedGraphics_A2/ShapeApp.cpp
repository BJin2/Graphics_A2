
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
//step 1 Add camera header to use camera class
#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "Waves.h"
#include <cmath>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// An enum to define primitive types and it will be used as index of draw argument when building the geometry.
enum PrimitiveType
{
	Box = 0,
	Cylinder,
	GeoSphere,
	Cone,
	Pyramid,
	Prism,
	Wedge,
	Diamond,
	PentaCylinder,
	Count
};

// An array of strings so save all draw arguments that is going to be used.
// Its order is the same as the PrimitiveType because PrimitiveType is used as index when building the geometry.
const std::string drawArgs[(int)PrimitiveType::Count] = { "box", "cylinder","geosphere","cone","pyramid","prism","wedge", "diamond", "penta" };


//create a string array
// An array of strings to contain all texture names which are going to be used in finding material type.
const std::string textureName[7] = { "wall", "roof", "deco0", "deco1", "deco2", "floor",  "door" };

// A simple struct that stores draw argument string and transform information
// Datas from text file will be stored in this format
struct DrawableItem
{
	DrawableItem() {}
	DrawableItem(std::string _type, XMFLOAT3 _position, XMFLOAT3 _rotation, XMFLOAT3 _scale) :
		type(_type), position(_position), rotation(_rotation), scale(_scale)
	{}
	std::string type;
	std::string tex;
	XMFLOAT3 position;
	XMFLOAT3 rotation;
	XMFLOAT3 scale;
};

// A struct that stores geometry informations that is used in BuildShapeGeometry function
// By storing these informations, building shapes can be done in loop.
// So that I don't have to hard code all primitives
struct Primitive
{
	GeometryGenerator::MeshData mesh;
	UINT vertexOffset;
	UINT indexOffset;
	SubmeshGeometry sub;
};

// Vectors to store structs
// itemList stores all the informations about actual castle.
// primitives stores geometry datas that is used in BuildShapeGeometry function
std::vector<DrawableItem> itemList;
std::vector<Primitive> primitives;

// To save sphere position to set each position of point light.
std::vector<XMFLOAT3> spherePos;

struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//step 13 create instance data vector
	std::vector<InstanceData> Instances;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	// step 14 create variable for instance number
	UINT InstanceCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	// step 15
	// Removed UpdateObjectCBs and created
	// update functions for material buffer and instance data
	void UpdateInstanceData(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildTestObjects();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayouts();
	void BuildCastleGeometry();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildTreeSpritesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	UINT mInstanceCount = 0;

	PassConstants mMainPassCB;

	// step 2
	// Removed all members related to angles and projection and 
	// added camera instead
	Camera mCamera;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShapesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException & e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// step 3 set camera position
	mCamera.SetPosition(0.0f, 2.0f, -15.0f);
	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures();
	BuildTestObjects();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayouts();
	BuildCastleGeometry();
	BuildWavesGeometry();
	BuildTreeSpritesGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	//step 4 Setting Camera lens(view) when window resized
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	// step 16 Calling update functions for material buffer and instance data
	UpdateInstanceData(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
	UpdateWaves(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// step 17
	// Bind all materials used in the scene.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

	// step 18
	// Bind all textures used in the scene.
	mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// step 5 rotate the camera using angles calculated from mouse input
		// Change camera pitch so the camera rotates up and down
		mCamera.Pitch(dy);
		// Rotate the camera in world y axis so that the camera rotates left and right without rolling
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	// step 6 Move the camera with keyboard input
	//GetAsyncKeyState returns a short (2 bytes)

	//Walk function in camera class moves the camera back and forward
	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);


	//Strafe function in camera class moves the camera left and right
	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

// step 19
// Remove UpdateObjectCBs and implement UpdateInstanceData
void ShapesApp::UpdateInstanceData(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer.get();
	for (auto& e : mAllRitems)
	{
		const auto& instanceData = e->Instances;

		int visibleInstanceCount = 0;

		for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
			XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

			InstanceData data;
			XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
			data.MaterialIndex = instanceData[i].MaterialIndex;

			// Write the instance data to structured buffer for the visible objects.
			currInstanceBuffer->CopyData(visibleInstanceCount++, data);
		}

		e->InstanceCount = visibleInstanceCount;

		std::wostringstream outs;
		outs.precision(6);
		outs << L"Instancing Demo" <<
			L"    " << e->InstanceCount <<
			L" objects visible out of " << e->Instances.size();
		mMainWndCaption = outs.str();
	}
}

// step 20
// Remove UpdateMaterialCBs and implement UpdateMaterialBuffer
// Changing MaterialCB to MaterialBuffer
void ShapesApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			// texture index
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	// step 7
	// When updating pass buffer, now view and projection matrices don't need calculating in the update function manually.
	// User can get view and projection matrices from the camera without calculating manually.
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	// The same thing applies to position. User don't need to calculate camera position manually.
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	if (!spherePos.empty())
	{
		for (int i = 0; i < spherePos.size(); i++)
		{
			mMainPassCB.Lights[i + 3].Position = spherePos[i];
			mMainPassCB.Lights[i + 3].Strength = { 1.0f, 0.0f, 0.0f };
		}
	}

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}


void ShapesApp::LoadTextures()
{
	//Load texture from texture folder by using name of files and save values in unique pointer.
	//Check missing texture information by using ThrowIfFail macro.
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../../Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	// Loading textures for castle
#pragma region Castle Textures
	auto wallTex = std::make_unique<Texture>();
	wallTex->Name = "wallTex";
	wallTex->Filename = L"../../Textures/Wall.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), wallTex->Filename.c_str(),
		wallTex->Resource, wallTex->UploadHeap));

	auto roofTex = std::make_unique<Texture>();
	roofTex->Name = "roofTex";
	roofTex->Filename = L"../../Textures/Roof.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), roofTex->Filename.c_str(),
		roofTex->Resource, roofTex->UploadHeap));

	auto deco0Tex = std::make_unique<Texture>();
	deco0Tex->Name = "deco0Tex";
	deco0Tex->Filename = L"../../Textures/Deco0.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), deco0Tex->Filename.c_str(),
		deco0Tex->Resource, deco0Tex->UploadHeap));

	auto deco1Tex = std::make_unique<Texture>();
	deco1Tex->Name = "deco1Tex";
	deco1Tex->Filename = L"../../Textures/Deco1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), deco1Tex->Filename.c_str(),
		deco1Tex->Resource, deco1Tex->UploadHeap));

	auto deco2Tex = std::make_unique<Texture>();
	deco2Tex->Name = "deco2Tex";
	deco2Tex->Filename = L"../../Textures/Deco2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), deco2Tex->Filename.c_str(),
		deco2Tex->Resource, deco2Tex->UploadHeap));

	auto floorTex = std::make_unique<Texture>();
	floorTex->Name = "floorTex";
	floorTex->Filename = L"../../Textures/Floor.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), floorTex->Filename.c_str(),
		floorTex->Resource, floorTex->UploadHeap));

	auto doorTex = std::make_unique<Texture>();
	doorTex->Name = "doorTex";
	doorTex->Filename = L"../../Textures/Door.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), doorTex->Filename.c_str(),
		doorTex->Resource, doorTex->UploadHeap));
#pragma endregion

	//Save loaded texture values in mTexture array (Unordered_map) by using key.
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);

	// Save loaded castle texture values in mTexture array
#pragma region Save loaded castle teuxtures
	mTextures[wallTex->Name] = std::move(wallTex);
	mTextures[roofTex->Name] = std::move(roofTex);
	mTextures[deco0Tex->Name] = std::move(deco0Tex);
	mTextures[deco1Tex->Name] = std::move(deco1Tex);
	mTextures[deco2Tex->Name] = std::move(deco2Tex);
	mTextures[floorTex->Name] = std::move(floorTex);
	mTextures[doorTex->Name] = std::move(doorTex);
#pragma endregion
}


void ShapesApp::BuildTestObjects()
{
	//Read text files
	std::ifstream fin_castle(L"CastleData/CastleInfo.txt");// CastleInfo has data about shape
	if (!fin_castle)
	{
		MessageBox(0, L"CastleData/CastleInfo.txt not found.", 0, 0);
	}
	std::ifstream fin_transform(L"CastleData/TransformInfo.txt");// TransformInfo has data about transform
	if (!fin_transform)
	{
		MessageBox(0, L"CastleData/TransformInfo.txt not found.", 0, 0);
	}

	// read text file that has texture info(index of textureName array) corresponding to each castle objects
	std::ifstream fin_texture(L"CastleData/TextureInfo.txt");// TextureInfo has data about which texture to use
	if (!fin_texture)
	{
		MessageBox(0, L"CastleData/TextureInfo.txt not found.", 0, 0);
	}

	//Number of objects
	UINT ocount;

	fin_castle >> ocount;

	for (UINT i = 0; i < ocount; i++)
	{
		//Create DrawableItem
		DrawableItem temp;
		int argIndex;
		int texIndex;
		//Get the shape info which is the index of draw argument
		fin_castle >> argIndex;
		fin_texture >> texIndex;
		temp.type = drawArgs[argIndex];

		// Get texture info which will be used to identify material that has appropriate texture
		temp.tex = textureName[texIndex];

		//Get transform data from text file
		fin_transform >> temp.position.x >> temp.position.y >> temp.position.z;
		fin_transform >> temp.rotation.x >> temp.rotation.y >> temp.rotation.z;
		fin_transform >> temp.scale.x >> temp.scale.y >> temp.scale.z;

		if (temp.type == "geosphere")
		{
			// save sphere positions for point lights
			spherePos.push_back(temp.position);
		}

		//Add the DrawbleItem instance to itemList
		itemList.push_back(temp);
	}
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	// step 21
	// Change descriptor heap init parameter. We have 11 textures
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 11, 0, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	// step 22 Modify root signature to represent how we bind resources.
	slotRootParameter[0].InitAsShaderResourceView(0, 1);
	slotRootParameter[1].InitAsShaderResourceView(1, 1);
	slotRootParameter[2].InitAsConstantBufferView(0);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}


void ShapesApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	// Change NumDescriptors
	//It was 4 and we are adding 7 more textures so 4+7 = 11
	srvHeapDesc.NumDescriptors = 11; // Set NumDescriptors to number of Textures.
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Save loaded texture resources from mTexture array.
	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;

	// Castle textures form mTexture array
#pragma region Castle Textures
	auto wallTex = mTextures["wallTex"]->Resource;
	auto roofTex = mTextures["roofTex"]->Resource;
	auto deco0Tex = mTextures["deco0Tex"]->Resource;
	auto deco1Tex = mTextures["deco1Tex"]->Resource;
	auto deco2Tex = mTextures["deco2Tex"]->Resource;
	auto floorTex = mTextures["floorTex"]->Resource;
	auto doorTex = mTextures["doorTex"]->Resource;
#pragma endregion
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;

	//Create shader resource descritor and save grass texture in resource view.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// Save resources in descriptors.
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	// Save water texture in resource view
	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	// Save fence texture in resource view
	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	// Save castle textures in resource view
	// These 7 textures goes before treeArrayTex to reduce process
	// Changing descriptor settings for texture array and then change it back for texture is not optimized
#pragma region Castle Textures
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	// Save wall texture in resource view
	srvDesc.Format = wallTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(wallTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save roof texture in resource view
	srvDesc.Format = roofTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(roofTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save decoration 0 texture in resource view
	srvDesc.Format = deco0Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(deco0Tex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save decoration 1 texture in resource view
	srvDesc.Format = deco1Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(deco1Tex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save decoration 2 texture in resource view
	srvDesc.Format = deco2Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(deco2Tex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save door texture in resource view
	srvDesc.Format = doorTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(doorTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	// Save floor texture in resource view
	srvDesc.Format = floorTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(floorTex.Get(), &srvDesc, hDescriptor);
#pragma endregion

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	// Save tree texture in resource view
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);
}

void ShapesApp::BuildShadersAndInputLayouts()
{
	// Removed Fog to see everything clearly
	//We are doing alpha test for objects that uses alpha test pso
	const D3D_SHADER_MACRO defines[] =
	{
		"1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"NULL", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mStdInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

// Function that builds castle geometry from Assignment 1
void ShapesApp::BuildCastleGeometry()
{
	GeometryGenerator geoGen;
	auto totalVertexCount = 0;
	std::vector<std::uint16_t> indices;

	// Storing mesh datas in a loop that runs 9 times(Number of primitives that is going to be used in the castle)
	for (UINT i = 0; i < PrimitiveType::Count; i++)
	{
		Primitive p;

		//Store mesh data depend on its primitive type
		switch (i)
		{
		case PrimitiveType::Box:
			p.mesh = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
			break;
		case PrimitiveType::Cone:
			p.mesh = geoGen.CreateCone(1.0f, 1.0f, 32, 1);
			break;
		case PrimitiveType::Cylinder:
			p.mesh = geoGen.CreateCylinder(1.0f, 1.0f, 1.0f, 32, 1);
			break;
		case PrimitiveType::GeoSphere:
			p.mesh = geoGen.CreateGeosphere(1.0f, 3);
			break;
		case PrimitiveType::Prism:
			p.mesh = geoGen.CreatePrism(1.0f, 1.0f, 1);
			break;
		case PrimitiveType::Pyramid:
			p.mesh = geoGen.CreatePyramid(1.0f, 1.0f, 1);
			break;
		case PrimitiveType::Wedge:
			p.mesh = geoGen.CreateWedge(1.0f, 1.0f, 1.0f, 1);
			break;
		case PrimitiveType::Diamond:
			p.mesh = geoGen.CreateDiamond(1.0f, 1.0f, 6);
			break;
		case PrimitiveType::PentaCylinder:
			p.mesh = geoGen.CreatePentaCylinder(1.0f, 1.0f, 1.0f, 1);
			break;
		}

		//If its the first one, offsets are 0
		if (i == 0)
		{
			p.vertexOffset = 0;
			p.indexOffset = 0;
		}
		else// From second object, offsets are previous offset + previous vertices/indices size
		{
			p.vertexOffset = primitives[i - 1].vertexOffset + (UINT)primitives[i - 1].mesh.Vertices.size();
			p.indexOffset = primitives[i - 1].indexOffset + (UINT)primitives[i - 1].mesh.Indices32.size();
		}

		//Storing submesh informations
		p.sub.IndexCount = (UINT)p.mesh.Indices32.size();
		p.sub.StartIndexLocation = p.indexOffset;
		p.sub.BaseVertexLocation = p.vertexOffset;

		//number of total vertex.
		//This will be used as size of a vector of Vetex that stores all vertices
		totalVertexCount += p.mesh.Vertices.size();

		//Adding indices
		indices.insert(indices.end(), std::begin(p.mesh.GetIndices16()), std::end(p.mesh.GetIndices16()));

		//Adding primitive into primitives which will be used for passing vertex positions and sub meshes
		primitives.push_back(p);
	}

	std::vector<Vertex> vertices(totalVertexCount);

	// Loop through all vertices and copy vertex positions from primitives
	// and copy color from color array that is declared at the top
	UINT k = 0;
	for (UINT i = 0; i < primitives.size(); i++)
	{
		for (UINT j = 0; j < primitives[i].mesh.Vertices.size(); j++, k++)
		{
			vertices[k].Pos = primitives[i].mesh.Vertices[j].Position;
			vertices[k].Normal = primitives[i].mesh.Vertices[j].Normal;
			vertices[k].TexC = primitives[i].mesh.Vertices[j].TexC;
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);



	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);



	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	// For each primitive, map the submesh with drawArgs(string)
	for (int i = 0; i < primitives.size(); i++)
	{
		geo->DrawArgs[drawArgs[i]] = primitives[i].sub;
	}

	mGeometries[geo->Name] = std::move(geo);
}


// Build wave geometry using Wave class
void ShapesApp::BuildWavesGeometry()
{
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}


void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;

	//there is abug with F2 key that is supposed to turn on the multisampling!
//Set4xMsaaState(true);
	//m4xMsaaState = true;

	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	// blend descriptor which has informations about blending
	//We are going to do additive blending here
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for tree sprites
	// To render quads from point we use geometry shader

	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};

	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, mInstanceCount, (UINT)mMaterials.size()));
	}
}


void ShapesApp::BuildMaterials()
{
	// Create unique pointer to save material information.
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	// Create unique pointer to save material information.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	// Create unique pointer to save material information.
	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;

	//Castle materials goes before tree material to match the order(same order as srvHeapDesc)
	//Create unique pointer to save castle material info
	for (int i = 3; i < 10; i++)
	{
		auto mat = std::make_unique<Material>();
		//Using textureName array as material name so that it can be matched with info in drawable item that is loaded from text file
		mat->Name = textureName[i - 3];
		mat->MatCBIndex = i;
		mat->DiffuseSrvHeapIndex = i;
		mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		mat->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);

		// Save information in mMaterial array (Unordered_map) by using textureName array for the same reason.
		mMaterials[textureName[i - 3]] = std::move(mat);
	}

	// Create unique pointer to save material information.
	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 10;
	treeSprites->DiffuseSrvHeapIndex = 10;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	// Save information in mMaterial array (Unordered_map) by using key.
	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);
}


void ShapesApp::BuildRenderItems()
{
	// Create unique pointer to save rendering information.
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mWavesRitem = wavesRitem.get();
	//Save information in mRitemLayer array by using (int)RenderLayer::Transparent key.
	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());
	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 3;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	//Save information in mRitemLayer array by using (int)RenderLayer::AlphaTestedTreeSprites key.
	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());




	//Build render item for castle
	for (size_t i = 0; i < itemList.size(); i++)
	{
		auto Ritem = std::make_unique<RenderItem>();

		XMMATRIX transform = XMMatrixIdentity();
		transform *= XMMatrixScaling(itemList[i].scale.x, itemList[i].scale.y, itemList[i].scale.z);
		transform *= XMMatrixRotationZ(XMConvertToRadians(itemList[i].rotation.z));
		transform *= XMMatrixRotationX(XMConvertToRadians(itemList[i].rotation.x));
		transform *= XMMatrixRotationY(XMConvertToRadians(itemList[i].rotation.y));
		transform *= XMMatrixTranslation(itemList[i].position.x, itemList[i].position.y, itemList[i].position.z);

		XMStoreFloat4x4(&Ritem->World, transform);

		Ritem->ObjCBIndex = i + 4;

		//Assign material for castle render items
		Ritem->Mat = mMaterials[itemList[i].tex].get();
		Ritem->Geo = mGeometries["shapeGeo"].get();

		Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		Ritem->IndexCount = Ritem->Geo->DrawArgs[itemList[i].type].IndexCount;
		Ritem->InstanceCount = 1;
		Ritem->StartIndexLocation = Ritem->Geo->DrawArgs[itemList[i].type].StartIndexLocation;
		Ritem->BaseVertexLocation = Ritem->Geo->DrawArgs[itemList[i].type].BaseVertexLocation;
		Ritem->Instances.resize(Ritem->InstanceCount);
		XMStoreFloat4x4(&Ritem->Instances[0].World, transform);
		XMStoreFloat4x4(&Ritem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
		Ritem->Instances[0].MaterialIndex = 4;
		mInstanceCount++;
		//Save information in mRitemLayer array by using (int)RenderLayer::Opaque key.
		mRitemLayer[(int)RenderLayer::Opaque].push_back(Ritem.get());
		//Save ObjCBIndex, mat, geo, transform, rotation and scale information in mAllRitems array (vector) to be readyto update object later.
		mAllRitems.push_back(std::move(Ritem));
	}

	//Save ObjCBIndex, mat, geo, transform, rotation and scale information in mAllRitems array (vector) to be ready to update object later.
	mAllRitems.push_back(std::move(wavesRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));

}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		auto instanceBuffer = mCurrFrameResource->InstanceBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress());

		//one item with 125 instances
		cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

float ShapesApp::GetHillsHeight(float x, float z)const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}
