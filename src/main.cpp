//
// Created by Gegel85 on 31/10/2020
//

#include <fstream>
#include <sstream>
#include <optional>
#include <dinput.h>
#include <nlohmann/json.hpp>
#include <SokuLib.hpp>
#include <shlwapi.h>
#include <thread>

#ifndef _DEBUG
#define puts(...)
#define printf(...)
#endif

#define BOXES_ALPHA 0.25
#define ASSIST_BOX_Y 429
#define LEFT_ASSIST_BOX_X 57
#define RIGHT_ASSIST_BOX_X 475
#define LEFT_BAR SokuLib::Vector2i{82, 422}
#define RIGHT_BAR SokuLib::Vector2i{479, 422}
#define LEFT_CROSS SokuLib::Vector2i{108, 417}
#define RIGHT_CROSS SokuLib::Vector2i{505, 417}


// Contructor:
// void FUN_0047f070(HudPlayerState *param_1, char param_1_00, CDesignBase *param_3, CDesignBase *param_4, int param_5)
// FUN_0047f070(&this->p1state, 0,(CDesignBase *)&this->field_0x98,(CDesignBase *)&this->field_0xcc,&this->field_0x4);
// Init:
// void FUN_0047ede0(HudPlayerState *param_1, Player *param_1_00)
// FUN_0047ede0(this, player);
struct HudPlayerState {
	char offset_0x00[0xC];
	unsigned hp;
	unsigned lastHp;
	char offset_0x14[0x4];
	SokuLib::CharacterManager *player;
	char offset_0x1C[0xDC];
};

// Contructor: 
// Init:
// void FUN_00435f10(HudPlayerStateUnknown2 *this, DeckInfo *deckInfo, int index)
// FUN_00435f10(this, &player->deckInfo, playerIndex);
struct HudPlayerState2 {
	SokuLib::DeckInfo *deck;
	char offset_0x000[0x150];
};

// Contructor:
// Init:
// void FUN_00478c50 (HudPlayerState3 *this, CDesignBase *obj, Player *player)
// FUN_00478c50(this_00, (CDesignBase *)&this->field_0x100, player);
struct HudPlayerState3 {
	void *designObject;
	SokuLib::CharacterManager *player;
	char offset_0x000[0x2C];
};

struct CInfoManager {
	void **vtable;
	char offset_0x004[0x168];
	HudPlayerState3 p1State3;
	HudPlayerState3 p2State3;
	HudPlayerState2 p1State2;
	HudPlayerState2 p2State2;
	SokuLib::Sprite *p1Portrait;
	SokuLib::Sprite *p2Portrait;
	char offset_0x484[0x14];
	HudPlayerState p1State;
	HudPlayerState p2State;
};

static_assert(offsetof(CInfoManager, p1State3) == 0x16C);
static_assert(offsetof(CInfoManager, p2State3) == 0x1A0);
static_assert(offsetof(CInfoManager, p1Portrait) == 0x47C);
static_assert(offsetof(CInfoManager, p2Portrait) == 0x480);
static_assert(offsetof(CInfoManager, p1State) == 0x498);
static_assert(offsetof(CInfoManager, p2State) == 0x590);

static_assert(offsetof(HudPlayerState, hp) == 0xC);
static_assert(offsetof(HudPlayerState, lastHp) == 0x10);
static_assert(offsetof(HudPlayerState, player) == 0x18);


struct GameDataManager {
	// 0x00
	SokuLib::List<std::pair<int, SokuLib::PlayerInfo*>> createQueue;
	// 0x0C
	bool isThreadsRunning; // align 3
	// 0x10
	uint32_t threadHandleA; // 0x46e6d0 
	uint32_t threadIdA;
	uint32_t threadHandleB; // 0x46e6f0 THREAD_PRIORITY_IDLE
	uint32_t threadIdB;
	uint32_t eventHandleA;
	uint32_t eventHandleB;
	// 0x28
	SokuLib::CharacterManager* players[4];
	// 0x38
	bool enabledPlayers[4]; // 01 01 00 00
	// 0x3C
	SokuLib::Vector<SokuLib::CharacterManager*> activePlayers;
	// 0x4C
	SokuLib::List<SokuLib::CharacterManager*> destroyQueue;
}; // 0x58

struct Coord { float x, y, z; };
struct SpriteEx {
	void *vtable;
	int dxHandle = 0;
	SokuLib::DxVertex vertices[4];
	SokuLib::Vector2f size;
	Coord baseCoords[4];
	Coord transfCoords[4];
	SokuLib::Vector2f size2;

	void setTexture(int texture, int texOffsetX, int texOffsetY, int width, int height, int anchorX, int anchorY);
	void setTexture(int texture, int texOffsetX, int texOffsetY, int width, int height);
	void clearTransform();
	void render();
	void render(float r, float g, float b);
	void render(float a, float r, float g, float b);
};
void SpriteEx::setTexture(int texture, int texOffsetX, int texOffsetY, int width, int height, int anchorX, int anchorY) {
	(this->*SokuLib::union_cast<void(SpriteEx::*)(int, int, int, int, int, int, int)>(0x406c60))(texture, texOffsetX, texOffsetY, width, height, anchorX, anchorY);
}
void SpriteEx::setTexture(int texture, int texOffsetX, int texOffsetY, int width, int height) {
	(this->*SokuLib::union_cast<void(SpriteEx::*)(int, int, int, int, int)>(0x41f7f0))(texture, texOffsetX, texOffsetY, width, height);
}
void SpriteEx::clearTransform() { (this->*SokuLib::union_cast<void(SpriteEx::*)()>(0x406ea0))(); }
void SpriteEx::render() { (this->*SokuLib::union_cast<void(SpriteEx::*)()>(0x4075d0))(); }
void SpriteEx::render(float r, float g, float b) { (this->*SokuLib::union_cast<void(SpriteEx::*)(float, float, float)>(0x7fb080))(r, g, b); }
void SpriteEx::render(float a, float r, float g, float b) { (this->*SokuLib::union_cast<void(SpriteEx::*)(float, float, float, float)>(0x7fb150))(a, r, g, b); }

struct SWRCMDINFO {
	bool enabled;
	int prev; // number representing the previously pressed buttons (masks are applied)
	int now; // number representing the current pressed buttons (masks are applied)

	struct {
		bool enabled;
		int id[10];
		int base; // once len reaches 10 (first cycle), is incremented modulo 10
		int len; // starts at 0, caps at 10
	} record;
};

struct RivControl {
	bool enabled;
	int texID;
	int forwardCount;
	int forwardStep;
	int forwardIndex;
	SWRCMDINFO cmdp1;
	SWRCMDINFO cmdp2;
	bool hitboxes;
	bool untech;
	bool show_debug;
	bool paused;
};

static unsigned char (__fastcall *og_advanceFrame)(SokuLib::CharacterManager *);
static int (SokuLib::BattleWatch::*ogBattleWatchOnProcess)();
static int (SokuLib::Select::*ogSelectOnProcess)();
static int (SokuLib::SelectClient::*ogSelectCLOnProcess)();
static int (SokuLib::SelectServer::*ogSelectSVOnProcess)();
static int (SokuLib::BattleManager::*ogBattleMgrOnProcess)();
static void (SokuLib::BattleManager::*ogBattleMgrOnRender)();
static void (__fastcall *og_handleInputs)(SokuLib::CharacterManager *);
static void (__fastcall *ogBattleMgrHandleCollision)(SokuLib::BattleManager*, int, void*, SokuLib::CharacterManager*);
static void (__stdcall *s_origLoadDeckData)(char *, void *, SokuLib::DeckInfo &, int, SokuLib::Dequeue<short> &);
static void (*og_FUN4098D6)(int, int, int, int);

static GameDataManager*& dataMgr = *(GameDataManager**)SokuLib::ADDR_GAME_DATA_MANAGER;

struct CEffectManager {
	void **vtable;
	void **vtable2;
	char offset_0x8[0x90];
};

struct ExtraChrSelectData {
	SokuLib::CDesign::Object *name;
	SokuLib::CDesign::Object *portrait;
	SokuLib::CDesign::Object *spellCircle;
	SokuLib::CDesign::Object *charObject;
	SokuLib::CDesign::Object *deckObject;
	SokuLib::CDesign::Sprite *profileBack;
	SokuLib::CDesign::Object *gear;
	SokuLib::CDesign::Sprite *cursor;
	SokuLib::CDesign::Sprite *deckSelect;
	SokuLib::CDesign::Sprite *colorSelect;
	SokuLib::v2::AnimationObject *object;
	SokuLib::KeymapManager *input;
	SokuLib::InputHandler chrHandler;
	SokuLib::InputHandler palHandler;
	SokuLib::InputHandler deckHandler;
	SokuLib::Sprite portraitSprite;
	SokuLib::Sprite circleSprite;
	SokuLib::Sprite gearSprite;
	CEffectManager effectMgr;
	int portraitTexture = 0;
	int circleTexture = 0;
	int selectState;
	int charNameCounter;
	int portraitCounter;
	int cursorCounter;
	int deckIndCounter;
	int chrCounter;
	bool needInit;
	bool isInit = false;
};

//static SokuLib::CharacterManager *obj[0xC] = {nullptr};
static char modFolder[1024];
static char soku2Dir[MAX_PATH];
static SokuLib::DrawUtils::RectangleShape rectangle;
static bool spawned = false;
static bool init = false;
static bool disp = false;
static bool anim = false;
static HMODULE myModule;
static SokuLib::DrawUtils::Sprite cursors[4];
CInfoManager hud2;
bool hudInit = false;
ExtraChrSelectData chrSelectExtra[2];
static SokuLib::KeymapManager *chrSelectExtraInputs[2] = {nullptr, nullptr};

auto sokuRand = (int (*)(int max))0x4099F0;
auto getCharName = (char *(*)(int))0x43F3F0;
auto setRenderMode = [](int mode) {
	((void (__thiscall *)(int, int))0x404B80)(0x896B4C, mode);
};
auto InputHandler_HandleInput = (bool (__thiscall *)(SokuLib::InputHandler &))0x41FBF0;
auto getChrName = (char *(*)(SokuLib::Character))0x43F3F0;
auto getInputManager = (SokuLib::KeymapManager *(*)(int index))0x43E040;
auto getInputManagerIndex = (char (*)(int index))0x43E070;
auto initInputManagerArray = (SokuLib::KeyManager *(*)(int index, bool))0x43E6A0;

static std::pair<SokuLib::KeymapManager, SokuLib::KeymapManager> keymaps;
static std::pair<SokuLib::KeyManager, SokuLib::KeyManager> keys{{&keymaps.first}, {&keymaps.second}};
static std::pair<SokuLib::PlayerInfo, SokuLib::PlayerInfo> assists = {
	SokuLib::PlayerInfo{SokuLib::CHARACTER_REIMU, 0, 0, 0, 0, {}, &keys.first},
	SokuLib::PlayerInfo{SokuLib::CHARACTER_REIMU, 1, 0, 0, 0, {}, &keys.second}
};

static void drawBox(const SokuLib::Box &box, const SokuLib::RotationBox *rotation, SokuLib::Color borderColor, SokuLib::Color fillColor)
{
	if (!rotation) {
		SokuLib::DrawUtils::FloatRect rect{};

		rect.x1 = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.left);
		rect.x2 = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.right);
		rect.y1 = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.top);
		rect.y2 = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.bottom);
		rectangle.setRect(rect);
	} else {
		SokuLib::DrawUtils::Rect<SokuLib::Vector2f> rect;

		rect.x1.x = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.left);
		rect.x1.y = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.top);

		rect.y1.x = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.left + rotation->pt1.x);
		rect.y1.y = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.top + rotation->pt1.y);

		rect.x2.x = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.left + rotation->pt1.x + rotation->pt2.x);
		rect.x2.y = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.top + rotation->pt1.y + rotation->pt2.y);

		rect.y2.x = SokuLib::camera.scale * (SokuLib::camera.translate.x + box.left + rotation->pt2.x);
		rect.y2.y = SokuLib::camera.scale * (SokuLib::camera.translate.y + box.top + rotation->pt2.y);
		rectangle.rawSetRect(rect);
	}

	rectangle.setFillColor(fillColor);
	rectangle.setBorderColor(borderColor);
	rectangle.draw();
}

static void drawCollisionBox(const SokuLib::ObjectManager &manager)
{
	SokuLib::DrawUtils::FloatRect rect{};
	const SokuLib::Box &box = *manager.frameData->collisionBox;

	if (!manager.frameData->collisionBox)
		return;

	rect.x1 = SokuLib::camera.scale * (std::ceil(manager.position.x) + manager.direction * box.left + SokuLib::camera.translate.x);
	rect.x2 = SokuLib::camera.scale * (std::ceil(manager.position.x) + manager.direction * box.right + SokuLib::camera.translate.x);
	rect.y1 = SokuLib::camera.scale * (box.top - std::ceil(manager.position.y) + SokuLib::camera.translate.y);
	rect.y2 = SokuLib::camera.scale * (box.bottom - std::ceil(manager.position.y) + SokuLib::camera.translate.y);

	rectangle.setRect(rect);
	rectangle.setFillColor(SokuLib::Color::Yellow * BOXES_ALPHA);
	rectangle.setBorderColor(SokuLib::Color::Yellow);
	rectangle.draw();
}

static void drawPositionBox(const SokuLib::ObjectManager &manager)
{
	rectangle.setPosition({
		static_cast<int>(SokuLib::camera.scale * (manager.position.x - 2 + SokuLib::camera.translate.x)),
		static_cast<int>(SokuLib::camera.scale * (-manager.position.y - 2 + SokuLib::camera.translate.y))
	});
	rectangle.setSize({
		static_cast<unsigned int>(SokuLib::camera.scale * 5),
		static_cast<unsigned int>(SokuLib::camera.scale * 5)
	});
	rectangle.setFillColor(SokuLib::Color::White);
	rectangle.setBorderColor(SokuLib::Color::White + SokuLib::Color::Black);
	rectangle.draw();
}

static void drawHurtBoxes(const SokuLib::ObjectManager &manager)
{
	if (manager.hurtBoxCount > 5)
		return;

	for (int i = 0; i < manager.hurtBoxCount; i++)
		drawBox(
			manager.hurtBoxes[i],
			manager.hurtBoxesRotation[i],
			SokuLib::Color::Green,
			(manager.frameData->frameFlags.chOnHit ? SokuLib::Color::Cyan : SokuLib::Color::Green) * BOXES_ALPHA
		);
}

static void drawHitBoxes(const SokuLib::ObjectManager &manager)
{
	if (manager.hitBoxCount > 5)
		return;

	for (int i = 0; i < manager.hitBoxCount; i++)
		drawBox(manager.hitBoxes[i], manager.hitBoxesRotation[i], SokuLib::Color::Red, SokuLib::Color::Red * BOXES_ALPHA);
}

static void drawPlayerBoxes(const SokuLib::CharacterManager &manager, bool playerBoxes = true)
{
	if (playerBoxes) {
		drawCollisionBox(manager.objectBase);
		drawHurtBoxes(manager.objectBase);
		drawHitBoxes(manager.objectBase);
		drawPositionBox(manager.objectBase);
	}

	auto array = manager.objects.list.vector();

	for (const auto _elem : array) {
		auto elem = reinterpret_cast<const SokuLib::ProjectileManager *>(_elem);

		if ((elem->isActive && elem->objectBase.hitCount > 0) || elem->objectBase.frameData->attackFlags.value > 0) {
			drawHurtBoxes(elem->objectBase);
			drawHitBoxes(elem->objectBase);
			drawPositionBox(elem->objectBase);
		}
	}
}

void updateObject(SokuLib::CharacterManager *main, SokuLib::CharacterManager *mgr)
{
	if (mgr->timeStop && !main->timeStop)
		main->timeStop = 1;
}

static int ctr = 0;

int __fastcall CBattleManager_OnProcess(SokuLib::BattleManager *This)
{
	auto players = (SokuLib::CharacterManager**)((int)This + 0x0C);

	if (This->matchState == -1)
		return (This->*ogBattleMgrOnProcess)();
	//if (players[0]->objectBase.hp == 0 && players[2]->objectBase.hp == 0 || players[1]->objectBase.hp == 0 && players[3]->objectBase.hp == 0)
	//	return SokuLib::SCENE_SELECT;
	if (SokuLib::checkKeyOneshot(DIK_F4, false, false, false))
		disp = !disp;
	if (!init) {
		puts("Init assisters");
		auto alloc = SokuLib::New<void *>(4);
		alloc[0] = dataMgr->activePlayers[0];
		alloc[1] = dataMgr->activePlayers[1];
		alloc[2] = dataMgr->players[2];
		alloc[3] = dataMgr->players[3];
		SokuLib::DeleteFct(*(void **)(*(int *)SokuLib::ADDR_GAME_DATA_MANAGER + 0x40));
		*(void **)(*(int *)SokuLib::ADDR_GAME_DATA_MANAGER + 0x40) = alloc;
		*(void **)(*(int *)SokuLib::ADDR_GAME_DATA_MANAGER + 0x44) = &alloc[4];

		dataMgr->players[2]->objectBase.opponent = &This->rightCharacterManager;
		dataMgr->players[3]->objectBase.opponent = &This->leftCharacterManager;
		for (int i = 0; i < 4; i++)
			dataMgr->players[i]->objectBase.offset_0x14E[0] = i;
		dataMgr->players[0]->keyManager->keymapManager->bindingSelect = 6;
		init = true;
	}

	int ret = (This->*ogBattleMgrOnProcess)();

	for (int i = 0; i < 4; i++) {
		if (dataMgr->players[i]->keyManager->keymapManager->input.select == 1) {
			int j = (i % 2) ^ 1;

			if (dataMgr->players[i]->objectBase.opponent == dataMgr->players[j])
				j += 2;
			printf("Switching %i's opponent to %i\n", i, j);
			dataMgr->players[i]->objectBase.opponent = dataMgr->players[j];
		}
	}
	if (!SokuLib::menuManager.empty() && SokuLib::sceneId == SokuLib::SCENE_BATTLE)
		return ret;
	if (!dataMgr->players[3])
		return ret;
	if (!dataMgr->players[2])
		return ret;

	*(bool *)0x89862E = true;
	if (SokuLib::checkKeyOneshot(DIK_F8, false, false, false) && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSCLIENT) {
		This->currentRound = (This->currentRound + 1) % 3;
		This->matchState = 0;
		anim = false;
	}

	auto left  = players[0];
	auto right = players[1];

	if (left->timeStop || right->timeStop) {
		if (players[2]->timeStop)
			updateObject(left, players[2]);
		if (players[3]->timeStop)
			updateObject(right, players[3]);
	} else {
		if (!players[3]->timeStop)
			updateObject(left, players[2]);
		if (!players[2]->timeStop)
			updateObject(right, players[3]);
	}

	if (players[2]->timeStop || players[3]->timeStop) {
		left->timeStop  = max(left->timeStop  + 1, 2);
		right->timeStop = max(right->timeStop + 1, 2);
	}

	if (This->matchState == 3) {
		players[0]->kdAnimationFinished = players[0]->kdAnimationFinished || players[2]->kdAnimationFinished;
		players[1]->kdAnimationFinished = players[1]->kdAnimationFinished || players[3]->kdAnimationFinished;
	} else {
		players[0]->kdAnimationFinished = players[0]->kdAnimationFinished && players[2]->kdAnimationFinished;
		players[1]->kdAnimationFinished = players[1]->kdAnimationFinished && players[3]->kdAnimationFinished;
	}
	players[0]->roundLost = players[0]->roundLost && players[2]->roundLost;
	players[1]->roundLost = players[1]->roundLost && players[3]->roundLost;
	players[2]->roundLost = players[0]->roundLost;
	players[3]->roundLost = players[1]->roundLost;
	players[2]->kdAnimationFinished = players[0]->kdAnimationFinished;
	players[3]->kdAnimationFinished = players[1]->kdAnimationFinished;
	return ret;
}

void selectCommon()
{
	auto &scene = SokuLib::currentScene->to<SokuLib::Select>();

	anim = false;
	if (spawned) {
		spawned = false;
		init = false;
	}

	if (SokuLib::checkKeyOneshot(DIK_F8, false, false, false) && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER) {
		printf("Changing assist for P1 to %i\n", SokuLib::leftChar);
		assists.first.character = SokuLib::leftChar;
		SokuLib::playSEWaveBuffer(0x28);
	}
	if (SokuLib::checkKeyOneshot(DIK_F8, false, false, false) && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSCLIENT) {
		printf("Changing assist for P2 to %i\n", SokuLib::rightChar);
		assists.second.character = SokuLib::rightChar;
		SokuLib::playSEWaveBuffer(0x28);
	}
}

int __fastcall CSelect_OnProcess(SokuLib::Select *This)
{
	int ret = (This->*ogSelectOnProcess)();

	selectCommon();
	return ret;
}

int __fastcall CSelectSV_OnProcess(SokuLib::SelectServer *This)
{
	int ret = (This->*ogSelectSVOnProcess)();

	selectCommon();
	return ret;
}

int __fastcall CSelectCL_OnProcess(SokuLib::SelectClient *This)
{
	int ret = (This->*ogSelectCLOnProcess)();

	selectCommon();
	return ret;
}

void __fastcall CBattleManager_HandleCollision(SokuLib::BattleManager* This, int unused, void* object, SokuLib::CharacterManager* character) {
	auto players = (SokuLib::CharacterManager**)((int)This + 0x0C);
	SokuLib::CharacterManager *assist;

	for (int i = 0; i < 4; i++)
		if (character == players[i]) {
			assist = players[((i / 2) ^ 1) * 2 + i % 2];
			break;
		}
	ogBattleMgrHandleCollision(This, unused, object, assist);
	ogBattleMgrHandleCollision(This, unused, object, character);
}

void __fastcall CBattleManager_OnRender(SokuLib::BattleManager *This)
{
	bool hasRIV = LoadLibraryA("ReplayInputView+") != nullptr;
	bool show = hasRIV ? ((RivControl *)((char *)This + sizeof(*This)))->hitboxes : disp;
	int offsets[] = {19, 15, 4, 28};

	(This->*ogBattleMgrOnRender)();
	if (This->matchState >= 2)
		for (int i = 3; i >= 0; i--) {
			auto op = dataMgr->players[i]->objectBase.opponent;

			cursors[i].setPosition((
				SokuLib::camera.translate * SokuLib::camera.scale +
				SokuLib::Vector2f{
					op->objectBase.position.x * SokuLib::camera.scale - offsets[i],
					-(op->objectBase.position.y + 200 + cursors[i].getSize().y) * SokuLib::camera.scale
				}).to<int>()
			);
			cursors[i].draw();
		}
	if (init && This->matchState < 6) {
		if (show && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSCLIENT) {
			if (!hasRIV) {
				drawPlayerBoxes(This->leftCharacterManager);
				drawPlayerBoxes(This->rightCharacterManager);
			}
			drawPlayerBoxes(*dataMgr->players[2]);
			drawPlayerBoxes(*dataMgr->players[3]);
		}
	}
}

const char *effectPath = "data/infoEffect/effect2.pat";
const char *battleUpperPath = "data/battle/battleUpper2.dat";
const char *battleUnderPath = "data/battle/battleUnder2.dat";

void initHud()
{
	DWORD old;

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	memset((void *)0x47E9CA, 0x90, 6);
	puts("Construct HUD");
	*(const char **)0x47DEC2 = effectPath;
	*(const char **)0x47DEE5 = battleUpperPath;
	*(const char **)0x47DEFD = battleUnderPath;
	((void (__thiscall *)(CInfoManager *))0x47EAF0)(&hud2);
	*(int *)0x47DEC2 = 0x85B430;
	*(int *)0x47DEE5 = 0x85B46C;
	*(int *)0x47DEFD = 0x85B450;

	SokuLib::CharacterManager** players = (SokuLib::CharacterManager**)((int)&SokuLib::getBattleMgr() + 0xC);
	auto p1 = players[0];
	auto p2 = players[1];
	auto _p1 = dataMgr->players[0];
	auto _p2 = dataMgr->players[1];

	puts("Init HUD");
	players[0] = players[2];
	players[1] = players[3];
	dataMgr->players[0] = dataMgr->players[2];
	dataMgr->players[1] = dataMgr->players[3];
	//*(char *)0x47E29C = 0x14;
	((void (__thiscall *)(CInfoManager *))0x47E260)(&hud2);
	//*(char *)0x47E29C = 0xC;
	players[0] = p1;
	players[1] = p2;
	dataMgr->players[0] = _p1;
	dataMgr->players[1] = _p2;
	hud2.p1Portrait->pos.y -= 54;
	hud2.p2Portrait->pos.y -= 54;
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);
}

int oldHud = 0;

void __declspec(naked) getOgHud()
{
	__asm {
		MOV ECX, [oldHud]
		RET
	}
}

void __declspec(naked) saveOldHud()
{
	__asm {
		MOV EAX, 0x8985E8
		MOV EAX, [EAX]
		MOV [oldHud], EAX
		RET
	}
}

void __declspec(naked) restoreOldHud()
{
	__asm {
		MOV EAX, [oldHud]
		PUSH EBX
		MOV EBX, 0x8985E8
		MOV [EBX], EAX
		POP EBX
		RET
	}
}

void __declspec(naked) swapStuff()
{
	__asm {
		CMP EDI, 2
		PUSH EBX
		MOV EBX, 0x8985E8
		JL swap2

		LEA EAX, hud2
		MOV [EBX], EAX
		POP EBX
		RET
	swap2:
		MOV EAX, [oldHud]
		MOV [EBX], EAX
		POP EBX
		RET
	}
}

void __fastcall updateCollisionBoxes(SokuLib::BattleManager *This)
{
	auto fct = (void (__thiscall *)(SokuLib::BattleManager *))0x47B840;
	SokuLib::CharacterManager** players = (SokuLib::CharacterManager**)((int)&SokuLib::getBattleMgr() + 0xC);
	float speeds[4] = {0, 0, 0, 0};

	// P1 - P2
	fct(This);
	speeds[0] += players[0]->additionalSpeed.x;
	speeds[1] += players[1]->additionalSpeed.x;
	// P3 - P2
	players[0] = dataMgr->players[2];
	fct(This);
	speeds[2] += players[0]->additionalSpeed.x;
	speeds[1] += players[1]->additionalSpeed.x;
	// P3 - P4
	players[1] = dataMgr->players[3];
	fct(This);
	speeds[2] += players[0]->additionalSpeed.x;
	speeds[3] += players[1]->additionalSpeed.x;
	// P1 - P4
	players[0] = dataMgr->players[0];
	fct(This);
	speeds[0] += players[0]->additionalSpeed.x;
	speeds[3] += players[1]->additionalSpeed.x;
	players[1] = dataMgr->players[1];
	for (int i = 0; i < 4; i++)
		players[i]->additionalSpeed.x = speeds[i];
}

void __stdcall loadDeckData(char *charName, void *csvFile, SokuLib::DeckInfo &deck, int param4, SokuLib::Dequeue<short> &newDeck)
{
	if (!spawned || init) {
		if (spawned)
			init = false;
		spawned = true;
		if (!cursors[0].texture.hasTexture()) {
			const char *cursorSprites[] = {
				"data/scene/select/character/04c_cursor1p.bmp",
				"data/scene/select/character/04c_cursor2p.bmp",
				"data/scene/select/character/04c_cursor3p.bmp",
				"data/scene/select/character/04c_cursor4p.bmp"
			};
			for (int i = 0; i < 4; i++) {
				cursors[i].texture.loadFromGame(cursorSprites[i]);
				cursors[i].setSize(cursors[i].texture.getSize());
				cursors[i].rect.width = cursors[i].getSize().x;
				cursors[i].rect.height = cursors[i].getSize().y;
			}
		}

		SokuLib::CharacterManager** players = (SokuLib::CharacterManager**)((int)&SokuLib::getBattleMgr() + 0xC);

		puts("Not spawned. Loading both assisters");
		puts("Loading character 1");
		if (assists.first.character == SokuLib::CHARACTER_RANDOM)
			assists.first.character = static_cast<SokuLib::Character>(sokuRand(20));
		((void (__thiscall *)(GameDataManager*, int, SokuLib::PlayerInfo &))0x46da40)(dataMgr, 2, assists.first);
		(*(void (__thiscall **)(SokuLib::CharacterManager *))(*(int *)dataMgr->players[2] + 0x44))(dataMgr->players[2]);
		players[2] = dataMgr->players[2];

		puts("Loading character 2");
		if (assists.second.character == SokuLib::CHARACTER_RANDOM)
			assists.second.character = static_cast<SokuLib::Character>(sokuRand(20));
		((void (__thiscall *)(GameDataManager*, int, SokuLib::PlayerInfo &))0x46da40)(dataMgr, 3, assists.second);
		(*(void (__thiscall **)(SokuLib::CharacterManager *))(*(int *)dataMgr->players[3] + 0x44))(dataMgr->players[3]);
		players[3] = dataMgr->players[3];

		init = false;
		printf("%p %p\n", dataMgr->players[2], dataMgr->players[3]);
		// Init
		//if (hudInit)
		//	((void (__thiscall *)(CInfoManager *, bool))*hud2.vtable)(&hud2, 0);
		hudInit = true;
		initHud();
	}
	s_origLoadDeckData(charName, csvFile, deck, param4, newDeck);
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16]) {
	return true;
}

void __fastcall handlePlayerInputs(SokuLib::CharacterManager *This)
{
	og_handleInputs(This);
	if (dataMgr->players[2])
		og_handleInputs(dataMgr->players[2]);
	if (dataMgr->players[3])
		og_handleInputs(dataMgr->players[3]);
}

void loadExtraPlayerInputs(int a, int b, int c, int d)
{
	og_FUN4098D6(a, b, c, d);
	if (SokuLib::mainMode != SokuLib::BATTLE_MODE_VSPLAYER)
		return;

	keymaps.first.vtable = (void *)0x85844C;
	keymaps.first.isPlayer = 0x05;
	keymaps.first.bindingUp = -1;
	keymaps.first.bindingDown = -1;
	keymaps.first.bindingLeft = -1;
	keymaps.first.bindingRight = -1;
	keymaps.first.bindingA = 0;
	keymaps.first.bindingB = 2;
	keymaps.first.bindingC = 1;
	keymaps.first.bindingD = 14;
	keymaps.first.bindingChangeCard = 15;
	keymaps.first.bindingSpellCard = 3;
	keymaps.first.bindingPause = 7;
	keymaps.first.bindingSelect = 0;
	memset(&keymaps.first.unknown, 0, sizeof(keymaps.first.unknown));
	//memset(&keymaps.first.inKeys, 0, sizeof(keymaps.first.inKeys));
	//memset(&keymaps.first.outKeys, 0, sizeof(keymaps.first.outKeys));
	//keymaps.first.readInKeys = false;
	assists.first.effectiveDeck.clear();
	for (int i = 0; i < 20; i++)
		assists.first.effectiveDeck.push_back(i / 4);

	keymaps.second.vtable = (void *)0x85844C;
	keymaps.second.isPlayer = -1;
	keymaps.second.bindingUp = DIK_W;
	keymaps.second.bindingDown = DIK_S;
	keymaps.second.bindingLeft = DIK_A;
	keymaps.second.bindingRight = DIK_D;
	keymaps.second.bindingA = DIK_J;
	keymaps.second.bindingB = DIK_K;
	keymaps.second.bindingC = DIK_L;
	keymaps.second.bindingD = DIK_U;
	keymaps.second.bindingChangeCard = DIK_I;
	keymaps.second.bindingSpellCard = DIK_O;
	keymaps.second.bindingPause = DIK_TAB;
	keymaps.second.bindingSelect = 0;
	memset(&keymaps.second.unknown, 0, sizeof(keymaps.second.unknown));
	//memset(&keymaps.second.inKeys, 0, sizeof(keymaps.second.inKeys));
	//memset(&keymaps.second.outKeys, 0, sizeof(keymaps.second.outKeys));
	//keymaps.second.readInKeys = false;

	assists.second.effectiveDeck.clear();
	assists.second.effectiveDeck.push_back(200);
	assists.second.effectiveDeck.push_back(200);
	assists.second.effectiveDeck.push_back(200);
	assists.second.effectiveDeck.push_back(200);
	assists.second.effectiveDeck.push_back(201);
	assists.second.effectiveDeck.push_back(201);
	assists.second.effectiveDeck.push_back(201);
	assists.second.effectiveDeck.push_back(201);
	assists.second.effectiveDeck.push_back(203);
	assists.second.effectiveDeck.push_back(203);
	assists.second.effectiveDeck.push_back(203);
	assists.second.effectiveDeck.push_back(203);
	assists.second.effectiveDeck.push_back(206);
	assists.second.effectiveDeck.push_back(206);
	assists.second.effectiveDeck.push_back(206);
	assists.second.effectiveDeck.push_back(206);
	assists.second.effectiveDeck.push_back(204);
	assists.second.effectiveDeck.push_back(204);
	assists.second.effectiveDeck.push_back(204);
	assists.second.effectiveDeck.push_back(204);
}

void __fastcall onDeath(SokuLib::CharacterManager *This)
{
	auto players = (SokuLib::CharacterManager**)((int)&SokuLib::getBattleMgr() + 0x0C);
	SokuLib::CharacterManager *p1 = players[0];
	SokuLib::CharacterManager *p2 = players[2];

	if (This != p1 && This != p2) {
		p1 = players[1];
		p2 = players[3];
	}
	p1->roundLost = p1->objectBase.hp == 0 && p2->objectBase.hp == 0;
	p2->roundLost = p1->roundLost;
}

unsigned char __fastcall checkWakeUp(SokuLib::CharacterManager *This)
{
	auto ret = og_advanceFrame(This);

	if (This->objectBase.hp == 0)
		ret = 0;
	return ret;
}

static void *og_switchBattleStateBreakAddr;

void __declspec(naked) healExtraChrs()
{
	__asm {
		MOV EAX, dword ptr [ESI + 0x14]
		MOV DX, word ptr [EAX + 0x186]
		MOV word ptr [EAX + 0x184],DX
		MOV EAX, dword ptr [ESI + 0x18]
		MOV DX, word ptr [EAX + 0x186]
		MOV word ptr [EAX + 0x184], DX
		JMP og_switchBattleStateBreakAddr
	}
}

static int (__thiscall *ogHudRender)(void *);

int __fastcall onHudRender(CInfoManager *This)
{
	if (SokuLib::getBattleMgr().matchState <= 5)
		ogHudRender(&hud2);

	int i = ogHudRender(This);

	if (SokuLib::getBattleMgr().matchState <= 5) {
		hud2.p1Portrait->render(0, 0);
		hud2.p2Portrait->render(640, 0);
		(**(void (__thiscall **)(void *, int, int, int))(*(int *)&hud2.offset_0x004[0x94] + 0x18))(&hud2.offset_0x004[0x94], 0, 0, 0x1A);
	}
	return i;
}

void __declspec(naked) updateOtherHud()
{
	__asm {
		LEA ECX, [hud2]
		MOV EAX, 0x47D6F0
		CALL EAX
		RET
	}
}

static int skipHealthRegen = 0x47B092;
static int dontSkipHealthRegen = 0x47B083;
static int extraHealing[2];

void __declspec(naked) setHealthRegen()
{
	__asm {
		MOVSX EAX, [ECX + 0x14E]
		CMP AL, 2
		JGE dontSkip
		MOV EDX, extraHealing
		LEA EAX, [EDX + EAX * 4]
		JMP skipHealthRegen
	dontSkip:
		JMP dontSkipHealthRegen
	}
}

void __declspec(naked) addHealthRegen()
{
	__asm {
		CALL SokuLib::getBattleMgr
		MOV EBX, [EAX + 0x14]
		MOV EDI, [EBX + 0x186]
		MOV ECX, [EBX + 0x68]
		ADD ECX, [extraHealing]
		CMP ECX, EDI
		JL lower1
		MOV ECX, EDI
	lower1:
		CMP ECX, 0x0
		JG higher1
		MOV ECX, 1
	higher1:
		MOV [EBX + 0x68], CX

		MOV EBX, [EAX + 0x18]
		MOV EDI, [EBX + 0x186]
		MOV ECX, [EBX + 0x68]
		ADD ECX, [extraHealing + 4]
		CMP ECX, EDI
		JL lower2
		MOV ECX, EDI
	lower2:
		CMP ECX, 0x0
		JG higher2
		MOV ECX, 1
	higher2:
		MOV [EBX + 0x68], CX
		RET
	}
}

void __declspec(naked) clearHealthRegen()
{
	__asm {
		MOV [extraHealing], 0
		MOV [extraHealing + 4], 0
		RET
	}
}

static SokuLib::Select *(__fastcall *og_SelectConstruct)(SokuLib::Select *This);

SokuLib::Select *__fastcall CSelect_construct(SokuLib::Select *This)
{
	og_SelectConstruct(This);

	int ret = 0;
	SokuLib::Vector2i size;
	char buffer[128];

	for (int i = 0; i < 2; i++) {
		auto &dat = chrSelectExtra[i];
		auto &profileInfo = (&assists.first)[i];

		if (dat.isInit) {
			((void (__thiscall *)(CEffectManager  *))0x4241B0)(&dat.effectMgr);
			SokuLib::textureMgr.deallocate(dat.portraitTexture);
			SokuLib::textureMgr.deallocate(dat.circleTexture);
		}
		This->designBase3.getById(&dat.name,        100 + i + 2);
		This->designBase3.getById(&dat.portrait,    900 + i + 2);
		This->designBase3.getById(&dat.spellCircle, 800 + i + 2);
		This->designBase3.getById(&dat.charObject,  700 + i + 2);
		This->designBase3.getById(&dat.deckObject,  720 + i + 2);
		This->designBase3.getById(&dat.profileBack, 300 + i + 2);
		This->designBase3.getById(&dat.gear,        600 + i + 2);
		This->designBase3.getById(&dat.cursor,      400 + i + 2);
		This->designBase3.getById(&dat.deckSelect,  320 + i + 2);
		This->designBase3.getById(&dat.colorSelect, 310 + i + 2);
		dat.cursor->active = true;
		dat.gear->active = true;
		dat.profileBack->active = true;
		dat.deckSelect->active = true;
		dat.colorSelect->active = true;
		// data/scene/select/character/06b_wheel%dp.bmp
		sprintf(buffer, (char *)0x857C18, i + 3);
		if (!SokuLib::textureMgr.loadTexture(&ret, buffer, &size.x, &size.y))
			printf("Failed to load %s\n", buffer);
		dat.gearSprite.setTexture(ret, 0, 0, size.x, size.y, size.x / 2, size.y / 2);
		ret = 0;

		dat.needInit = true;
		dat.input = nullptr;
		dat.object = nullptr;
		dat.selectState = 0;
		dat.chrHandler.maxValue = 20;
		dat.chrHandler.pos = 10 - i;
		dat.chrHandler.posCopy = 10 - i;
		dat.palHandler.maxValue = 8;
		dat.palHandler.pos = 0;
		dat.palHandler.posCopy = 0;
		dat.deckHandler.maxValue = 4;
		dat.deckHandler.pos = 0;
		dat.deckHandler.posCopy = 0;
		dat.portraitCounter = 15;
		dat.charNameCounter = 15;
		dat.chrCounter = 15;
		dat.cursorCounter = 60;
		dat.cursor->x1 = 700 * i - 50;
		dat.isInit = true;
		((void (__thiscall *)(CEffectManager  *))0x422CF0)(&dat.effectMgr);
		profileInfo.character = *((SokuLib::Character *(__thiscall *)(const void *, int))0x420380)(&This->offset_0x018[0x80], dat.chrHandler.pos);
		profileInfo.palette = 1;
		profileInfo.deck = 0;
		// data/scene/select/character/09b_character/character_%02d.bmp
		sprintf(buffer, (char *)0x85785C, profileInfo.character);
		if (!SokuLib::textureMgr.loadTexture(&ret, buffer, &size.x, &size.y))
			printf("Failed to load %s\n", buffer);
		dat.portraitSprite.setTexture2(ret, 0, 0, size.x, size.y);
		dat.portraitTexture = ret;
	}
	return This;
}

void __declspec(naked) checkUsedInputs()
{
	__asm {
		MOV EAX, 0
	loop1:
		CMP EAX, ESI
		JE loopEnd
		JG normal
		MOV byte ptr [ESP + 0xC], 0x1
		JMP loop0
	normal:
		MOV byte ptr [ESP + 0xC], 0x0
	loop0:

		MOV CL, [EAX + 0x898678]
		CMP CL, [ESI + 0x898678]
		JNE loopEnd

		CMP byte ptr [ESP + 0xC], 0x0
		JE resetOther

		MOV byte ptr [ESI + 0x898678], 0xFE
		RET

	resetOther:
		MOV byte ptr [EAX + 0x898678], 0xFE
		CMP EAX, 2
		JGE extraData

		MOV [0x898680 + EAX * 4], 0
		JMP loopEnd

	extraData:
		DEC EAX
		DEC EAX
		MOV [chrSelectExtraInputs + EAX * 4], 0
		INC EAX
		INC EAX

	loopEnd:
		INC EAX
		CMP EAX, 4
		JL loop1
		RET
	}
}

void __declspec(naked) setInputPointer()
{
	__asm {
		CMP ESI, 2
		JGE extraData

		MOV [0x898680 + ESI * 4], EAX
		RET

	extraData:
		DEC ESI
		DEC ESI
		MOV [chrSelectExtraInputs + ESI * 4], EAX
		RET
	}
}

void __declspec(naked) cmpInputPointer()
{
	__asm {
		CMP ESI, 2
		JGE extraData

		CMP [0x898680 + ESI * 4], 0
		RET

	extraData:
		PUSH ESI
		DEC ESI
		DEC ESI
		CMP [chrSelectExtraInputs + ESI * 4], 0
		POP ESI
		RET
	}
}

void __declspec(naked) loadInputPointer()
{
	__asm {
		CMP EAX, 2
		JGE extraData

		MOV EAX, [0x898680 + EAX * 4]
		RET

	extraData:
		DEC EAX
		DEC EAX
		MOV EAX, [chrSelectExtraInputs + EAX * 4]
		RET
	}
}

void __fastcall initExtraInputsLight(SokuLib::Select *This, int b)
{
	auto keyboard = (SokuLib::KeymapManager *)0x8986A8;

	for (int i = 2 + b; i; i--) {
		if ((getInputManagerIndex(i - 1) & 0xFE) == 0xFE) {
			if (chrSelectExtra[b].selectState == 0) {
				if (i == 3)
					chrSelectExtra[0].input = keyboard;
				else if (i != 3)
					(&This->leftKeys)[i - 1] = keyboard;
			} else {
				if (i == 3)
					chrSelectExtra[0].input = nullptr;
				else if (i != 3)
					(&This->leftKeys)[i - 1] = nullptr;
			}

			if ((i == 3 && chrSelectExtra[0].selectState == 3) || (i != 3 && (&This->leftSelectionStage)[i - 1] == 3)) {
				chrSelectExtra[b].input = keyboard;
				if (chrSelectExtra[b].input && b == 0)
					initExtraInputsLight(This, b + 1);
			} else
				chrSelectExtra[b].input = nullptr;
			return;
		}
	}
	chrSelectExtra[b].input = nullptr;
}

void __fastcall initExtraInputs(SokuLib::Select *This, int b)
{
	if (!chrSelectExtra[b].needInit) {
		chrSelectExtra[b].input = getInputManager(2 + b);
		if (chrSelectExtra[b].input && b == 0)
			initExtraInputs(This, b + 1);
		return;
	}
	initInputManagerArray(2 + b, false);

	auto ptr = getInputManager(2 + b);

	if (ptr != nullptr) {
		chrSelectExtra[b].input = ptr;
		if (chrSelectExtra[b].input && b == 0)
			initExtraInputs(This, b + 1);
		return;
	}
	initExtraInputsLight(This, b);
}

void __declspec(naked) initExtraInputsLight_hook()
{
	__asm {
		MOV dword ptr [ESI + 0x14], EAX
		TEST EAX, EAX
		JZ ret_

		PUSH EAX
		PUSH EDI
		PUSH ECX
		PUSH EBX
		PUSH EDX
		PUSH ESI
		MOV ECX, ESI
		XOR EDX, EDX
		CALL initExtraInputsLight
		POP ESI
		POP EDX
		POP EBX
		POP ECX
		POP EDI
		POP EAX
	ret_:
		XOR EDI, EDI
		RET
	}
}

void __declspec(naked) initExtraInputs_hook()
{
	__asm {
		MOV dword ptr [ESI + 0x14], EAX
		TEST EAX, EAX
		JZ ret_

		PUSH EAX
		PUSH EDI
		PUSH ECX
		PUSH EBX
		PUSH EDX
		PUSH ESI
		MOV ECX, ESI
		XOR EDX, EDX
		CALL initExtraInputs
		POP ESI
		POP EDX
		POP EBX
		POP ECX
		POP EDI
		POP EAX
	ret_:
		CMP EAX, EDI
		RET
	}
}

void __fastcall renderChrSelectChrData(int index)
{
	auto &dat = chrSelectExtra[index];
	float offset;

	if (dat.portraitCounter <= 0)
		offset = 0;
	else if (dat.portraitCounter >= 0xF)
		offset = 1;
	else
		offset = (dat.portraitCounter * dat.portraitCounter) / (15.f * 15.f);
	dat.portraitSprite.render(offset * 640 + dat.portrait->x2, dat.portrait->y2);
}

void __fastcall renderChrSelectChrDataGear(int index)
{
	auto &dat = chrSelectExtra[index];

	dat.gearSprite.rotation = dat.cursor->x1 * 2.5;
	dat.gearSprite.render(dat.cursor->x1 + dat.gear->x2, dat.gear->y2);
}

void __fastcall renderChrSelectChrName(SokuLib::Select *This, int index)
{
	auto &dat = chrSelectExtra[index];
	auto &info = (&assists.first)[index];
	float offset;

	if (dat.charNameCounter <= 0)
		offset = 0;
	else if (dat.charNameCounter >= 0xF)
		offset = 1;
	else
		offset = (dat.charNameCounter * dat.charNameCounter) / (15.f * 15.f);
	This->charNameSprites[info.character == SokuLib::CHARACTER_RANDOM ? 20 : info.character].render(offset * 200 + dat.name->x2, dat.name->y2);
	if (dat.object) {
		if (dat.chrCounter <= 0)
			offset = 0;
		else if (dat.chrCounter >= 0xF)
			offset = 1;
		else
			offset = (dat.chrCounter * dat.chrCounter) / (15.f * 15.f);
		setRenderMode(2);
		if (index == 0)
			offset *= -1;
		dat.circleSprite.render(dat.spellCircle->x2, dat.spellCircle->y2 + 200 * offset);
		dat.object->position.y = dat.charObject->y2 + offset * 200;
		dat.object->render();
	}
}


void __declspec(naked) renderChrSelectChrData_hook()
{
	__asm {
		MOV ECX, [ESP + 0x1C]
		PUSH EAX
		PUSH ESI
		PUSH EDI
		PUSH EBX
		CALL renderChrSelectChrData
		POP EBX
		POP EDI
		POP ESI
		POP EAX
		CMP EAX, -3
		CMP [ESI + 0xFFFFFE4C], 0
		RET
	}
}

void __declspec(naked) renderChrSelectChrDataGear_hook()
{
	__asm {
		MOV ECX, [ESP + 0x1C]
		PUSH EAX
		PUSH ESI
		PUSH EDI
		PUSH EBX
		CALL renderChrSelectChrDataGear
		POP EBX
		POP EDI
		POP ESI
		POP EAX
		MOV ECX, [ESI + 0x148]
		RET
	}
}

void __declspec(naked) renderChrSelectChrName_hook()
{
	__asm {
		MOV ECX, EBX
		MOV EDX, ESI
		PUSH EAX
		PUSH ESI
		PUSH EDI
		PUSH EBX
		CALL renderChrSelectChrName
		POP EBX
		POP EDI
		POP ESI
		POP EAX
		INC ESI
		ADD EDI, 4
		RET
	}
}

int updateCharacterSelect_hook_failAddr = 0x42091D;
int updateCharacterSelect_hook_retAddr = 0x4208F4;

void updateCharacterSelect2(const SokuLib::Select *This, unsigned i)
{
	auto &dat = chrSelectExtra[i];
	char buffer[128];
	SokuLib::Vector2i size;
	float offset;
	auto &info = (&assists.first)[i];

	if (info.character != SokuLib::CHARACTER_RANDOM) {
		if (dat.portraitCounter)
			dat.portraitCounter--;
	} else {
		if (dat.portraitCounter < 15)
			dat.portraitCounter++;
	}
	if (dat.cursorCounter)
		dat.cursorCounter--;
	if (dat.charNameCounter)
		dat.charNameCounter--;

	if (dat.cursorCounter <= 0)
		dat.cursor->x1 = This->charPortraitStartX + This->charPortraitSliceWidth * dat.chrHandler.pos;
	else
		dat.cursor->x1 -= (dat.cursor->x1 - (This->charPortraitStartX + This->charPortraitSliceWidth * dat.chrHandler.pos)) / 6;
	if (dat.selectState == 0 || dat.selectState == 3) {
		if (dat.deckIndCounter)
			dat.deckIndCounter--;
	} else {
		if (dat.deckIndCounter < 15)
			dat.deckIndCounter++;
	}
	if (dat.selectState == 0) {
		if (dat.chrCounter < 15)
			dat.chrCounter++;
	} else {
		if (dat.chrCounter)
			dat.chrCounter--;
	}

	if (dat.deckIndCounter <= 0)
		offset = 0;
	else if (dat.deckIndCounter >= 0xF)
		offset = 1;
	else
		offset = (dat.deckIndCounter * dat.deckIndCounter) / (15.f * 15.f);

	if (i == 0)
		dat.colorSelect->y1 = 150 - offset * 150;
	else
		dat.colorSelect->y1 = -150 + offset * 150;
	dat.deckSelect->x1 = -280 + 280 * offset;
	dat.circleSprite.rotation += 0.5;
	if (dat.object)
		dat.object->advanceFrame();

	if (!dat.input)
		return;
	switch (dat.selectState) {
	case 0:
		dat.chrHandler.keys = &dat.input->input;
		if (InputHandler_HandleInput(dat.chrHandler)) {
			SokuLib::playSEWaveBuffer(0x27);
			info.character = *((SokuLib::Character *(__thiscall *)(const void *, int))0x420380)(&This->offset_0x018[0x80], dat.chrHandler.pos);
			sprintf(buffer, (char *)0x85785C, info.character);
			SokuLib::textureMgr.deallocate(dat.portraitTexture);
			if (!SokuLib::textureMgr.loadTexture(&dat.portraitTexture, buffer, &size.x, &size.y))
				printf("Failed to load %s\n", buffer);
			dat.portraitSprite.setTexture2(dat.portraitTexture, 0, 0, size.x, size.y);
			dat.portraitCounter = 15;
			dat.charNameCounter = 15;
			dat.cursorCounter = 30;
			break;
		}
		if (dat.input->input.a == 1) {
			char *name = getCharName(info.character);

			SokuLib::playSEWaveBuffer(0x28);
			dat.selectState = 1;
			sprintf(buffer, (char *)0x8578EC, info.character);
			SokuLib::textureMgr.deallocate(dat.circleTexture);
			if (!SokuLib::textureMgr.loadTexture(&dat.circleTexture, buffer, &size.x, &size.y))
				printf("Failed to load %s\n", buffer);
			dat.circleSprite.setTexture(dat.circleTexture, 0, 0, size.x, size.y, size.x / 2, size.y / 2);
			((void (__thiscall *)(CEffectManager *))dat.effectMgr.vtable[2])(&dat.effectMgr);
			sprintf(buffer, (char *)0x8578C8, name, info.palette);
			((void (__thiscall *)(int, char *, int, int))0x408BE0)(0x8A0048, buffer, 0x896B88, 0x10);
			sprintf(buffer, (char *)0x85789C, name);
			((void (__thiscall *)(CEffectManager *, char *, int))dat.effectMgr.vtable[1])(&dat.effectMgr, buffer, 0);
			dat.object = ((SokuLib::v2::AnimationObject *(__thiscall *)(CEffectManager *, int, float, float, int, int, int))dat.effectMgr.vtable[3])(&dat.effectMgr, 0, dat.charObject->x2, dat.charObject->y2, 1, 0, 0);
			dat.object->setPose(0);
		} else if (dat.input->input.b == 1) {

		} else if (dat.input->input.d == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			dat.selectState = 3;
			info.character = SokuLib::CHARACTER_RANDOM;
			dat.charNameCounter = 15;
		}
		break;
	case 1:
		if (dat.input->input.a == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			dat.object->setPose(1);
			dat.selectState = 3;
		} else if (dat.input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			dat.selectState = 0;
		}
	case 3:
		if (dat.input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			dat.selectState = 0;
			if (info.character == SokuLib::CHARACTER_RANDOM) {
				info.character = *((SokuLib::Character *(__thiscall *)(const void *, int))0x420380)(&This->offset_0x018[0x80], dat.chrHandler.pos);
				dat.charNameCounter = 15;
			}
		}
	}
}

bool updateCharacterSelect(const SokuLib::Select *This)
{
	updateCharacterSelect2(This, 0U);
	updateCharacterSelect2(This, 1U);
	return chrSelectExtra[0].selectState == 3 && chrSelectExtra[1].selectState == 3;
}

void __declspec(naked) updateCharacterSelect_hook()
{
	__asm {
		PUSH ESI
		CALL updateCharacterSelect
		POP ESI
		TEST EAX, EAX
		JNZ ok
		JMP updateCharacterSelect_hook_failAddr
	ok:
		CMP byte ptr [ESI + 0x22C0], 0x3
		JMP updateCharacterSelect_hook_retAddr
	}
}

void onCharacterSelectInit()
{
	chrSelectExtraInputs[0] = nullptr;
	chrSelectExtraInputs[1] = nullptr;
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	DWORD old;

#ifdef _DEBUG
	FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif

	myModule = hMyModule;
	GetModuleFileName(hMyModule, modFolder, 1024);
	PathRemoveFileSpec(modFolder);
	puts("Hello");
	// DWORD old;
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	ogBattleMgrOnRender  = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender,  CBattleManager_OnRender);
	ogBattleMgrOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onProcess, CBattleManager_OnProcess);
	ogSelectOnProcess = SokuLib::TamperDword(&SokuLib::VTable_Select.onProcess, CSelect_OnProcess);
	ogSelectCLOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onProcess, CSelectCL_OnProcess);
	ogSelectSVOnProcess = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onProcess, CSelectSV_OnProcess);
	ogHudRender = (int (__thiscall *)(void *))SokuLib::TamperDword(0x85b544, onHudRender);
	// s_origCLogo_OnProcess   = TamperDword(vtbl_CLogo   + 4, (DWORD)CLogo_OnProcess);
	// s_origCBattle_OnProcess = TamperDword(vtbl_CBattle + 4, (DWORD)CBattle_OnProcess);
	// s_origCBattleSV_OnProcess = TamperDword(vtbl_CBattleSV + 4, (DWORD)CBattleSV_OnProcess);
	// s_origCBattleCL_OnProcess = TamperDword(vtbl_CBattleCL + 4, (DWORD)CBattleCL_OnProcess);
	// s_origCTitle_OnProcess  = TamperDword(vtbl_CTitle  + 4, (DWORD)CTitle_OnProcess);
	// s_origCSelect_OnProcess = TamperDword(vtbl_CSelect + 4, (DWORD)CSelect_OnProcess);
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	ogBattleMgrHandleCollision = SokuLib::TamperNearJmpOpr(0x47d618, CBattleManager_HandleCollision);
	SokuLib::TamperNearJmpOpr(0x47d64c, CBattleManager_HandleCollision);
	// Enable 4 characters collision
	*(char *)0x47D1A1 = 0x4;
	*(char *)0x47D520 = 0x4;
	const unsigned char collisionPatch[] = {
		// MOV AL, [ESP+0x28]
		0x8A, 0x44, 0x24, 0x28,
		// CMP AL, 2
		0x3C, 0x02,
		// JNE 47D530
		0x75, 0x04,
		// SUB ESI, 0x18
		0x83, 0xEE, 0x18,
		// NOP
		0x90
	};
	memcpy((void*)0x47D524, collisionPatch, sizeof(collisionPatch));
	// JNZ 47D530 -> JNZ 47D524
	*(char *)0x47D5BF = 0x61;

	// Enable 4 characters inputs
	*(char *)0x48219D = 0x4;

	const unsigned char chrSelectInputInitPatch[] = {
		// MOV [00898684],EBX
		0x89, 0x1D, 0x84, 0x86, 0x89, 0x00,
		// NOP
		0x90,
		// MOV EBX, 00898678
		0xBB, 0x78, 0x86, 0x89, 0x00,
		// mov [ebx],FEFEFEFE
		0xC7, 0x03, 0xFE, 0xFE, 0xFE, 0xFE,
		// XOR EBX, EBX
		0x31, 0xDB
	};
	memcpy((void*)0x43EA13, chrSelectInputInitPatch, sizeof(chrSelectInputInitPatch));

	new SokuLib::Trampoline(0x4796EE, updateOtherHud, 5);

	og_switchBattleStateBreakAddr = (void *)SokuLib::TamperNearJmpOpr(0x481F58, healExtraChrs);
	og_advanceFrame = SokuLib::TamperNearJmpOpr(0x48535D, checkWakeUp);
	*(char *)0x481AAE = 0x8B;
	*(char *)0x481AAF = 0xC8;
	SokuLib::TamperNearCall(0x481AB0, onDeath);
	*(char *)0x47D6AC = 0;
	*(char *)0x47D6B8 = 0;

	const int callSites[] = {
		0x480970, 0x4809D0, 0x480A7B, 0x480B11, 0x480B95, 0x480DCD,
		0x48279A, 0x4827EA, 0x48283A, 0x4828AB, 0x482928, 0x4829AF
	};
	for (auto addr : callSites)
		SokuLib::TamperNearJmpOpr(addr, updateCollisionBoxes);

	SokuLib::TamperNearJmp(0x47B07C, setHealthRegen);
	*(char *)0x47B081 = 0x90;
	*(char *)0x47B082 = 0x90;
	new SokuLib::Trampoline(0x47D6E0, addHealthRegen, 7);
	new SokuLib::Trampoline(0x47D18E, clearHealthRegen, 7);

	SokuLib::TamperNearCall(0x43890E, getOgHud);
	*(char *)0x438913 = 0x90;

	new SokuLib::Trampoline(0x46DEC1, saveOldHud, 5);
	new SokuLib::Trampoline(0x46DEC6, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF02, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF44, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF80, swapStuff, 5);
	new SokuLib::Trampoline(0x46DFC0, swapStuff, 5);
	SokuLib::TamperNearJmp(0x46E002, restoreOldHud);

	// Filesystem first patch
	*(char *)0x40D1FB = 0xEB;
	*(char *)0x40D27A = 0x74;
	*(char *)0x40D27B = 0x91;
	*(char *)0x40D245 = 0x1C;
	memset((char *)0x40D27C, 0x90, 7);

	// Chr select input stuff
	SokuLib::TamperNearCall(0x43E6C8, checkUsedInputs);
	memset((void *)(0x43E6C8 + 5), 0x90, 0x43E70B - 0x43E6C8 - 5);
	*(char *)0x43E6CD = 0x8A;
	*(char *)0x43E6CE = 0x86;
	*(char *)0x43E6CF = 0x78;
	*(char *)0x43E6D0 = 0x86;
	*(char *)0x43E6D1 = 0x89;
	*(char *)0x43E6D2 = 0x00;
	*(char *)0x43E6D3 = 0x3C;
	*(char *)0x43E6D4 = 0xFE;
	*(char *)0x43E6D5 = 0x75;
	*(char *)0x43E6D6 = 0x02;
	*(char *)0x43E6D7 = 0x5E;
	*(char *)0x43E6D8 = 0xC3;
	*(int *)0x43E72D = 0x8986A8;
	SokuLib::TamperNearCall(0x43E731, setInputPointer);
	*(char *)0x43E736 = 0x90;
	SokuLib::TamperNearCall(0x43E723, setInputPointer);
	*(char *)0x43E728 = 0x90;
	*(char *)0x43E729 = 0x90;
	SokuLib::TamperNearCall(0x43E6A5, cmpInputPointer);
	*(char *)0x43E6AA = 0x90;
	*(char *)0x43E6AB = 0x90;
	*(char *)0x43E6AC = 0x90;
	SokuLib::TamperNearJmp(0x43E044, loadInputPointer);
	SokuLib::TamperNearCall(0x42289D, initExtraInputs_hook);
	SokuLib::TamperNearCall(0x422871, initExtraInputsLight_hook);

	new SokuLib::Trampoline(0x43EA0D, onCharacterSelectInit, 6);

	og_SelectConstruct = SokuLib::TamperNearJmpOpr(0x41E55F, CSelect_construct);

	SokuLib::TamperNearCall(0x42112C, renderChrSelectChrDataGear_hook);
	*(char *)0x421131 = 0x90;
	SokuLib::TamperNearCall(0x420DAD, renderChrSelectChrData_hook);
	*(char *)0x420DB2 = 0x90;
	*(char *)0x420DB3 = 0x90;
	SokuLib::TamperNearCall(0x4212D9, renderChrSelectChrName_hook);
	*(char *)0x4212DE = 0x90;

	SokuLib::TamperNearJmp(0x4208ED, updateCharacterSelect_hook);
	*(char *)0x4208F2 = 0x90;
	*(char *)0x4208F3 = 0x90;

	og_handleInputs = SokuLib::TamperNearJmpOpr(0x48224D, handlePlayerInputs);
	s_origLoadDeckData = SokuLib::TamperNearJmpOpr(0x437D23, loadDeckData);
	og_FUN4098D6 = SokuLib::TamperNearJmpOpr(0x43F136, loadExtraPlayerInputs);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}