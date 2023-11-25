#include <windows.h>
#include <string>
#include <sstream>

//Structs are reverse-engineered and probably inaccurate.
//Some names were recovered, most are guessed.


#pragma pack(1)
struct Player {
	uint32_t	vtable;
	uint32_t	playerController;	//Ptr to 'master' player controller (contains pointers to PlayerInputController, PlayerMovementController, PlayerCollisionController, PlayerFPSAnimator, PlayerMouseLookController)
	uint32_t	idk1;
	uint32_t	idk2;
	uint32_t	idk3;
	uint32_t	gridX;				//player's X grid position (0x14)
	uint32_t	gridY;				//player's Y grid position (0x18)
	float		posX;				//player's X position (0x1c)
	float		posY;				//player's Y position (Layer in 2d map, changes when jumping) (0x20)
	float		posZ;				//player's Z position (0x24)
	float		camPitch;			//Camera Pitch (I might have mixed pitch/yaw, idk) (0x28)
	float		camYaw;				//Camera Yaw (0x2c)
	float		camRoll;			//Camera Roll/SideTilt (0x30)
	float		idk4;				//Might be player radius? (0x34)
	uint8_t     idk5[104];
	int32_t		health;				//Player's health (0xa0)
	uint8_t     idk6[144];
	float		movementSpeed;		//Player movement speed (0x134)
	//I dunno nuffin' man
};

struct Md3
{
	uint8_t idk[0x28];
	float scale;
};

struct Decoration {
	uint8_t		idk1[0x1c];
	float		posX;
	float		posY;
	float		posZ;
	float		idk2;				//Might be pitch/roll/x/z rot?
	float		yaw;
	uint8_t		idk3[0x24];
	uint32_t	id;					//Seemingly unique decoration/entity id
	uint8_t		idk4[0x104];
	Md3* md3Ptr;				//Null when non-3d decoration
	float		offsetX;			//Md3 offsets
	float		offsetY;
	float		offsetZ;
};

//Level manager? Entity manager?
struct LvlMgr {
	Player* playerPtr;
	Decoration** decArray;
	uintptr_t		endDecArray;
};

struct GameState3D {
	uint8_t     idk1[0x08];
	float		screenFade;	//1 = black screen, 0 = no fade
	int			isLoading;	//Seems to be 1 when loading, 0 when in game
	uint8_t     idk2[0xa0];
	LvlMgr* lvlMgrPtr;
};

struct Transform {
	float x;
	float y;
	float z;
	int id;
};

struct Vector2 {
	float x;
	float y;
};

const uintptr_t gameStateOffset = 0x256EA4;

//Returns the index of the specified entity id in the transform array (needed because decorations are often shuffled in game's memory)
static int FindEntityTransform(Transform* array, int count, int id)
{
	for (int i = 0; i < count; i++)
	{
		if (array[i].id == id)
			return i;
	}

	return -1;	//Hopefully never happens?
}

//Rotate a 2d vector around a specified point, using the specified angle (radians)
static Vector2 RotateAroundPoint(Vector2 origin, Vector2 pos, float theta)
{
	float distX = pos.x - origin.x;
	float distY = pos.y - origin.y;

	float cosTheta = cosf(theta);
	float sinTheta = sinf(theta);

	Vector2 rotatedPoint;
	rotatedPoint.x = cosTheta * distX - sinTheta * distY;
	rotatedPoint.y = sinTheta * distX + cosTheta * distY;

	rotatedPoint.x += origin.x;
	rotatedPoint.y += origin.y;

	return rotatedPoint;
}

static Decoration** GetDecorationArray(int* decorationCount)
{
	GameState3D** gameStatePtr = (GameState3D**)((uintptr_t)GetModuleHandle("Game.exe") + gameStateOffset);
	if (!gameStatePtr)
		return 0;

	GameState3D* gameState = *gameStatePtr;

	if (!gameState || !gameState->lvlMgrPtr || !gameState->lvlMgrPtr->decArray || !decorationCount)
		return 0;

	*decorationCount = (gameState->lvlMgrPtr->endDecArray - (uintptr_t)gameState->lvlMgrPtr->decArray) / 4;

	return ((Decoration**)gameState->lvlMgrPtr->decArray);
}

static Player* GetPlayerPtr()
{
	GameState3D** gameStatePtr = (GameState3D**)((uintptr_t)GetModuleHandle("Game.exe") + gameStateOffset);
	if (!gameStatePtr)
		return 0;

	GameState3D* gameState = *gameStatePtr;

	if (!gameState || !gameState->lvlMgrPtr || !gameState->lvlMgrPtr->playerPtr)
		return 0;

	return gameState->lvlMgrPtr->playerPtr;
}

//Might not 100% accurate
static bool CheckIsLoading()
{
	GameState3D** gameStatePtr = (GameState3D**)((uintptr_t)GetModuleHandle("Game.exe") + gameStateOffset);
	if (!gameStatePtr)
		return true;

	GameState3D* gameState = *gameStatePtr;
	if (!gameState)
		return true;

	return gameState->isLoading || gameState->screenFade > 0.0f;
}

static float DegToRad(float angle)
{
	return (angle / 360.0f) * 6.283185f;
}

//Apply original md3 offsets for each 3d object, so we can overwrite them later without needing to consider the original offsets
static void ApplyBaseOffsets(Decoration** decorations, int count, Transform* transforms)
{
	for (int i = 0; i < count; i++)
	{
		//Skip non-3d decorations
		if (decorations[i]->md3Ptr == 0)
			continue;

		int id = decorations[i]->id;

		float scale = decorations[i]->md3Ptr->scale;

		Vector2 origin;
		origin.x = 0;
		origin.y = 0;

		Vector2 offPos;
		offPos.x = decorations[i]->offsetX * scale;
		offPos.y = -decorations[i]->offsetZ * scale;

		offPos = RotateAroundPoint(origin, offPos, DegToRad(decorations[i]->yaw));
		decorations[i]->posX += offPos.x;
		decorations[i]->posZ += offPos.y;
		decorations[i]->offsetX = 0;
		decorations[i]->offsetZ = 0;


		transforms[i].x = decorations[i]->posX;
		transforms[i].y = decorations[i]->posY;
		transforms[i].z = decorations[i]->posZ;
		transforms[i].id = id;
	}
}


DWORD WINAPI MainThread(HMODULE hModule)
{
	Decoration** prevDecorationArray = NULL;	//Previous address where the decoration array was stored
	Transform* decStartTransform = NULL;	//Original transform information for decorations, before we modify them.
	int prevDecCount = 0;	//Previous Decoration Count


	//When a level loads, the decoration array moves a lot.
	//These two variables are used to wait until decoration array has been at the same memory location for 100 iterations
	int stabilityCooldown = 0;
	bool needsUpdate = false;

	while (1)
	{
		Sleep(20);

		if (stabilityCooldown > 0)
			stabilityCooldown--;

		if (CheckIsLoading())
		{
			stabilityCooldown = 100;
			needsUpdate = true;
			continue;
		}

		int decorationCount = 0;
		Decoration** decorationArray = GetDecorationArray(&decorationCount);
		if (!decorationArray)
		{
			prevDecorationArray = NULL;
			stabilityCooldown = 100;
			needsUpdate = true;
			continue;
		}

		if (prevDecorationArray != decorationArray || prevDecCount != decorationCount)
		{
			needsUpdate = true;
			stabilityCooldown = 100;

			prevDecorationArray = decorationArray;
			prevDecCount = decorationCount;
		}

		if (needsUpdate && stabilityCooldown > 0)
			continue;

		//If decorations array is changed (every level load?), and has been stable for a whle, apply/save all the starting 3d model offsets.
		if (needsUpdate)
		{
			decStartTransform = (Transform*)realloc(decStartTransform, sizeof(Transform) * decorationCount);	//Can you tell I prefer C? (and live dangerously with my unchecked realloc calls)
			ApplyBaseOffsets(decorationArray, decorationCount, decStartTransform);
			needsUpdate = false;
		}


		Player* playerPtr = GetPlayerPtr();

		if (!playerPtr)
			continue;

		//Get player's yaw in radians
		float camRot = DegToRad(playerPtr->camYaw + 180.0f);

		//Calculate position in front of player
		float forwardX = sinf(camRot);
		float forwardZ = cosf(camRot);

		//Move it forward a little more, so it's not 'inside' the player.
		forwardX *= 10.0f;
		forwardZ *= 10.0f;

		for (int i = 0; i < decorationCount; i++)
		{
			//Only act on 3d models
			if (decorationArray[i]->md3Ptr == 0)
				continue;

			int transformIndex = FindEntityTransform(decStartTransform, decorationCount, decorationArray[i]->id);

			float scale = decorationArray[i]->md3Ptr->scale;	//Model offset is affected by scale

			//Move direction directly in front of player
			decorationArray[i]->posX = playerPtr->posX + forwardX;
			decorationArray[i]->posZ = -(playerPtr->posZ + forwardZ);

			//Set offset to the difference between the model's intended location, and the placed location (with model rotation applied)
			Vector2 origin;
			origin.x = 0;
			origin.y = 0;

			Vector2 offPos;
			offPos.x = (decStartTransform[transformIndex].x - decorationArray[i]->posX) / scale;
			offPos.y = (-decStartTransform[transformIndex].z + decorationArray[i]->posZ) / scale;
			offPos = RotateAroundPoint(origin, offPos, DegToRad(decorationArray[i]->yaw));

			decorationArray[i]->offsetX = offPos.x;
			decorationArray[i]->offsetZ = offPos.y;
		}

	}

	ExitThread(0);
}


//Everything below here is just the dll hijacking stuff, nothing EFPSE related.
//Made with https://github.com/maluramichael/dll-proxy-generator


struct hid_dll {
	HMODULE dll;
	FARPROC OrignalHidD_FlushQueue;
	FARPROC OrignalHidD_FreePreparsedData;
	FARPROC OrignalHidD_GetAttributes;
	FARPROC OrignalHidD_GetConfiguration;
	FARPROC OrignalHidD_GetFeature;
	FARPROC OrignalHidD_GetHidGuid;
	FARPROC OrignalHidD_GetIndexedString;
	FARPROC OrignalHidD_GetInputReport;
	FARPROC OrignalHidD_GetManufacturerString;
	FARPROC OrignalHidD_GetMsGenreDescriptor;
	FARPROC OrignalHidD_GetNumInputBuffers;
	FARPROC OrignalHidD_GetPhysicalDescriptor;
	FARPROC OrignalHidD_GetPreparsedData;
	FARPROC OrignalHidD_GetProductString;
	FARPROC OrignalHidD_GetSerialNumberString;
	FARPROC OrignalHidD_Hello;
	FARPROC OrignalHidD_SetConfiguration;
	FARPROC OrignalHidD_SetFeature;
	FARPROC OrignalHidD_SetNumInputBuffers;
	FARPROC OrignalHidD_SetOutputReport;
	FARPROC OrignalHidP_GetButtonCaps;
	FARPROC OrignalHidP_GetCaps;
	FARPROC OrignalHidP_GetData;
	FARPROC OrignalHidP_GetExtendedAttributes;
	FARPROC OrignalHidP_GetLinkCollectionNodes;
	FARPROC OrignalHidP_GetScaledUsageValue;
	FARPROC OrignalHidP_GetSpecificButtonCaps;
	FARPROC OrignalHidP_GetSpecificValueCaps;
	FARPROC OrignalHidP_GetUsageValue;
	FARPROC OrignalHidP_GetUsageValueArray;
	FARPROC OrignalHidP_GetUsages;
	FARPROC OrignalHidP_GetUsagesEx;
	FARPROC OrignalHidP_GetValueCaps;
	FARPROC OrignalHidP_InitializeReportForID;
	FARPROC OrignalHidP_MaxDataListLength;
	FARPROC OrignalHidP_MaxUsageListLength;
	FARPROC OrignalHidP_SetData;
	FARPROC OrignalHidP_SetScaledUsageValue;
	FARPROC OrignalHidP_SetUsageValue;
	FARPROC OrignalHidP_SetUsageValueArray;
	FARPROC OrignalHidP_SetUsages;
	FARPROC OrignalHidP_TranslateUsagesToI8042ScanCodes;
	FARPROC OrignalHidP_UnsetUsages;
	FARPROC OrignalHidP_UsageListDifference;
} hid;

__declspec(naked) void FakeHidD_FlushQueue() { _asm { jmp[hid.OrignalHidD_FlushQueue] } }
__declspec(naked) void FakeHidD_FreePreparsedData() { _asm { jmp[hid.OrignalHidD_FreePreparsedData] } }
__declspec(naked) void FakeHidD_GetAttributes() { _asm { jmp[hid.OrignalHidD_GetAttributes] } }
__declspec(naked) void FakeHidD_GetConfiguration() { _asm { jmp[hid.OrignalHidD_GetConfiguration] } }
__declspec(naked) void FakeHidD_GetFeature() { _asm { jmp[hid.OrignalHidD_GetFeature] } }
__declspec(naked) void FakeHidD_GetHidGuid() { _asm { jmp[hid.OrignalHidD_GetHidGuid] } }
__declspec(naked) void FakeHidD_GetIndexedString() { _asm { jmp[hid.OrignalHidD_GetIndexedString] } }
__declspec(naked) void FakeHidD_GetInputReport() { _asm { jmp[hid.OrignalHidD_GetInputReport] } }
__declspec(naked) void FakeHidD_GetManufacturerString() { _asm { jmp[hid.OrignalHidD_GetManufacturerString] } }
__declspec(naked) void FakeHidD_GetMsGenreDescriptor() { _asm { jmp[hid.OrignalHidD_GetMsGenreDescriptor] } }
__declspec(naked) void FakeHidD_GetNumInputBuffers() { _asm { jmp[hid.OrignalHidD_GetNumInputBuffers] } }
__declspec(naked) void FakeHidD_GetPhysicalDescriptor() { _asm { jmp[hid.OrignalHidD_GetPhysicalDescriptor] } }
__declspec(naked) void FakeHidD_GetPreparsedData() { _asm { jmp[hid.OrignalHidD_GetPreparsedData] } }
__declspec(naked) void FakeHidD_GetProductString() { _asm { jmp[hid.OrignalHidD_GetProductString] } }
__declspec(naked) void FakeHidD_GetSerialNumberString() { _asm { jmp[hid.OrignalHidD_GetSerialNumberString] } }
__declspec(naked) void FakeHidD_Hello() { _asm { jmp[hid.OrignalHidD_Hello] } }
__declspec(naked) void FakeHidD_SetConfiguration() { _asm { jmp[hid.OrignalHidD_SetConfiguration] } }
__declspec(naked) void FakeHidD_SetFeature() { _asm { jmp[hid.OrignalHidD_SetFeature] } }
__declspec(naked) void FakeHidD_SetNumInputBuffers() { _asm { jmp[hid.OrignalHidD_SetNumInputBuffers] } }
__declspec(naked) void FakeHidD_SetOutputReport() { _asm { jmp[hid.OrignalHidD_SetOutputReport] } }
__declspec(naked) void FakeHidP_GetButtonCaps() { _asm { jmp[hid.OrignalHidP_GetButtonCaps] } }
__declspec(naked) void FakeHidP_GetCaps() { _asm { jmp[hid.OrignalHidP_GetCaps] } }
__declspec(naked) void FakeHidP_GetData() { _asm { jmp[hid.OrignalHidP_GetData] } }
__declspec(naked) void FakeHidP_GetExtendedAttributes() { _asm { jmp[hid.OrignalHidP_GetExtendedAttributes] } }
__declspec(naked) void FakeHidP_GetLinkCollectionNodes() { _asm { jmp[hid.OrignalHidP_GetLinkCollectionNodes] } }
__declspec(naked) void FakeHidP_GetScaledUsageValue() { _asm { jmp[hid.OrignalHidP_GetScaledUsageValue] } }
__declspec(naked) void FakeHidP_GetSpecificButtonCaps() { _asm { jmp[hid.OrignalHidP_GetSpecificButtonCaps] } }
__declspec(naked) void FakeHidP_GetSpecificValueCaps() { _asm { jmp[hid.OrignalHidP_GetSpecificValueCaps] } }
__declspec(naked) void FakeHidP_GetUsageValue() { _asm { jmp[hid.OrignalHidP_GetUsageValue] } }
__declspec(naked) void FakeHidP_GetUsageValueArray() { _asm { jmp[hid.OrignalHidP_GetUsageValueArray] } }
__declspec(naked) void FakeHidP_GetUsages() { _asm { jmp[hid.OrignalHidP_GetUsages] } }
__declspec(naked) void FakeHidP_GetUsagesEx() { _asm { jmp[hid.OrignalHidP_GetUsagesEx] } }
__declspec(naked) void FakeHidP_GetValueCaps() { _asm { jmp[hid.OrignalHidP_GetValueCaps] } }
__declspec(naked) void FakeHidP_InitializeReportForID() { _asm { jmp[hid.OrignalHidP_InitializeReportForID] } }
__declspec(naked) void FakeHidP_MaxDataListLength() { _asm { jmp[hid.OrignalHidP_MaxDataListLength] } }
__declspec(naked) void FakeHidP_MaxUsageListLength() { _asm { jmp[hid.OrignalHidP_MaxUsageListLength] } }
__declspec(naked) void FakeHidP_SetData() { _asm { jmp[hid.OrignalHidP_SetData] } }
__declspec(naked) void FakeHidP_SetScaledUsageValue() { _asm { jmp[hid.OrignalHidP_SetScaledUsageValue] } }
__declspec(naked) void FakeHidP_SetUsageValue() { _asm { jmp[hid.OrignalHidP_SetUsageValue] } }
__declspec(naked) void FakeHidP_SetUsageValueArray() { _asm { jmp[hid.OrignalHidP_SetUsageValueArray] } }
__declspec(naked) void FakeHidP_SetUsages() { _asm { jmp[hid.OrignalHidP_SetUsages] } }
__declspec(naked) void FakeHidP_TranslateUsagesToI8042ScanCodes() { _asm { jmp[hid.OrignalHidP_TranslateUsagesToI8042ScanCodes] } }
__declspec(naked) void FakeHidP_UnsetUsages() { _asm { jmp[hid.OrignalHidP_UnsetUsages] } }
__declspec(naked) void FakeHidP_UsageListDifference() { _asm { jmp[hid.OrignalHidP_UsageListDifference] } }

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	char path[MAX_PATH];
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		//Load the real hid.dll, and get the locations of all its exports, so we can proxy them to Game.exe
		CopyMemory(path + GetSystemDirectory(path, MAX_PATH - 9), "\\hid.dll", 10);
		hid.dll = LoadLibrary(path);
		if (!hid.dll)
		{
			MessageBox(0, "Cannot load original hid.dll library", "Proxy", MB_ICONERROR);
			ExitProcess(0);
		}
		hid.OrignalHidD_FlushQueue = GetProcAddress(hid.dll, "HidD_FlushQueue");
		hid.OrignalHidD_FreePreparsedData = GetProcAddress(hid.dll, "HidD_FreePreparsedData");
		hid.OrignalHidD_GetAttributes = GetProcAddress(hid.dll, "HidD_GetAttributes");
		hid.OrignalHidD_GetConfiguration = GetProcAddress(hid.dll, "HidD_GetConfiguration");
		hid.OrignalHidD_GetFeature = GetProcAddress(hid.dll, "HidD_GetFeature");
		hid.OrignalHidD_GetHidGuid = GetProcAddress(hid.dll, "HidD_GetHidGuid");
		hid.OrignalHidD_GetIndexedString = GetProcAddress(hid.dll, "HidD_GetIndexedString");
		hid.OrignalHidD_GetInputReport = GetProcAddress(hid.dll, "HidD_GetInputReport");
		hid.OrignalHidD_GetManufacturerString = GetProcAddress(hid.dll, "HidD_GetManufacturerString");
		hid.OrignalHidD_GetMsGenreDescriptor = GetProcAddress(hid.dll, "HidD_GetMsGenreDescriptor");
		hid.OrignalHidD_GetNumInputBuffers = GetProcAddress(hid.dll, "HidD_GetNumInputBuffers");
		hid.OrignalHidD_GetPhysicalDescriptor = GetProcAddress(hid.dll, "HidD_GetPhysicalDescriptor");
		hid.OrignalHidD_GetPreparsedData = GetProcAddress(hid.dll, "HidD_GetPreparsedData");
		hid.OrignalHidD_GetProductString = GetProcAddress(hid.dll, "HidD_GetProductString");
		hid.OrignalHidD_GetSerialNumberString = GetProcAddress(hid.dll, "HidD_GetSerialNumberString");
		hid.OrignalHidD_Hello = GetProcAddress(hid.dll, "HidD_Hello");
		hid.OrignalHidD_SetConfiguration = GetProcAddress(hid.dll, "HidD_SetConfiguration");
		hid.OrignalHidD_SetFeature = GetProcAddress(hid.dll, "HidD_SetFeature");
		hid.OrignalHidD_SetNumInputBuffers = GetProcAddress(hid.dll, "HidD_SetNumInputBuffers");
		hid.OrignalHidD_SetOutputReport = GetProcAddress(hid.dll, "HidD_SetOutputReport");
		hid.OrignalHidP_GetButtonCaps = GetProcAddress(hid.dll, "HidP_GetButtonCaps");
		hid.OrignalHidP_GetCaps = GetProcAddress(hid.dll, "HidP_GetCaps");
		hid.OrignalHidP_GetData = GetProcAddress(hid.dll, "HidP_GetData");
		hid.OrignalHidP_GetExtendedAttributes = GetProcAddress(hid.dll, "HidP_GetExtendedAttributes");
		hid.OrignalHidP_GetLinkCollectionNodes = GetProcAddress(hid.dll, "HidP_GetLinkCollectionNodes");
		hid.OrignalHidP_GetScaledUsageValue = GetProcAddress(hid.dll, "HidP_GetScaledUsageValue");
		hid.OrignalHidP_GetSpecificButtonCaps = GetProcAddress(hid.dll, "HidP_GetSpecificButtonCaps");
		hid.OrignalHidP_GetSpecificValueCaps = GetProcAddress(hid.dll, "HidP_GetSpecificValueCaps");
		hid.OrignalHidP_GetUsageValue = GetProcAddress(hid.dll, "HidP_GetUsageValue");
		hid.OrignalHidP_GetUsageValueArray = GetProcAddress(hid.dll, "HidP_GetUsageValueArray");
		hid.OrignalHidP_GetUsages = GetProcAddress(hid.dll, "HidP_GetUsages");
		hid.OrignalHidP_GetUsagesEx = GetProcAddress(hid.dll, "HidP_GetUsagesEx");
		hid.OrignalHidP_GetValueCaps = GetProcAddress(hid.dll, "HidP_GetValueCaps");
		hid.OrignalHidP_InitializeReportForID = GetProcAddress(hid.dll, "HidP_InitializeReportForID");
		hid.OrignalHidP_MaxDataListLength = GetProcAddress(hid.dll, "HidP_MaxDataListLength");
		hid.OrignalHidP_MaxUsageListLength = GetProcAddress(hid.dll, "HidP_MaxUsageListLength");
		hid.OrignalHidP_SetData = GetProcAddress(hid.dll, "HidP_SetData");
		hid.OrignalHidP_SetScaledUsageValue = GetProcAddress(hid.dll, "HidP_SetScaledUsageValue");
		hid.OrignalHidP_SetUsageValue = GetProcAddress(hid.dll, "HidP_SetUsageValue");
		hid.OrignalHidP_SetUsageValueArray = GetProcAddress(hid.dll, "HidP_SetUsageValueArray");
		hid.OrignalHidP_SetUsages = GetProcAddress(hid.dll, "HidP_SetUsages");
		hid.OrignalHidP_TranslateUsagesToI8042ScanCodes = GetProcAddress(hid.dll, "HidP_TranslateUsagesToI8042ScanCodes");
		hid.OrignalHidP_UnsetUsages = GetProcAddress(hid.dll, "HidP_UnsetUsages");
		hid.OrignalHidP_UsageListDifference = GetProcAddress(hid.dll, "HidP_UsageListDifference");

		//Start our custom code in a new thread
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);

		break;
	}
	case DLL_PROCESS_DETACH:
	{
		FreeLibrary(hid.dll);
	}
	break;
	}
	return TRUE;
}
