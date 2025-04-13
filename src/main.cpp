//
// Created by PinkySmile on 31/10/2020
//

#define _USE_MATH_DEFINES 1
#include <random>
#include <fstream>
#include <sstream>
#include <optional>
#include <dinput.h>
#include <nlohmann/json.hpp>
#include <SokuLib.hpp>
#include <shlwapi.h>
#include <thread>
#include <zlib.h>
#include <iostream>


#ifndef _DEBUG
#define puts(...)
#define printf(...)
#endif

#define TAG_CALL_METER 150
#define ASSIST_CALL_METER 75
#define ASSIST_CARD_METER 750
#define MAX_METER_REQ 1000
#define START_BURST_CHARGES 2
#define ASSIST_METER_CONVERSION 1
#define SLOW_TAG_STARTUP 60
#define TAG_CD 300
#define CROSS_TAG_CD 900
#define GROUND_TAG_CD 600
#define SLOW_TAG_CD 900
#define SLOW_TAG_COST 2
#define FONT_HEIGHT 16
#define TEXTURE_SIZE 0x200
#define BOXES_ALPHA 0.25
#define ASSIST_BOX_Y 429
#define LEFT_ASSIST_BOX_X 57
#define RIGHT_ASSIST_BOX_X 475
#define LEFT_BAR SokuLib::Vector2i{82, 422}
#define RIGHT_BAR SokuLib::Vector2i{479, 422}
#define LEFT_CROSS SokuLib::Vector2i{108, 417}
#define RIGHT_CROSS SokuLib::Vector2i{505, 417}

#define dashTimer gpShort[0]

struct PacketGameMatchEvent {
	SokuLib::PacketType opcode;
	SokuLib::GameType type;
	uint8_t data[sizeof(SokuLib::PlayerMatchData) * 4 + 256 * 4 + 7];

	SokuLib::PlayerMatchData &operator[](int i)
	{
		if (i == 0)
			return *(SokuLib::PlayerMatchData *)this->data;
		return *(SokuLib::PlayerMatchData *)(*this)[i - 1].getEndPtr();
	}

	uint8_t &loadouts(int i)
	{
		return (*this)[3].getEndPtr()[i];
	}

	uint8_t &stageId()
	{
		return (*this)[3].getEndPtr()[4];
	}

	uint8_t &musicId()
	{
		return (*this)[3].getEndPtr()[5];
	}

	uint32_t &randomSeed()
	{
		return *reinterpret_cast<uint32_t *>(&(*this)[3].getEndPtr()[6]);
	}

	uint8_t &matchId()
	{
		return (*this)[3].getEndPtr()[10];
	}

	size_t getSize() const
	{
		return ((ptrdiff_t)&this->matchId() + 1) - (ptrdiff_t)this;
	}

	const SokuLib::PlayerMatchData &operator[](int i) const
	{
		if (i == 0)
			return *(SokuLib::PlayerMatchData *)this->data;
		return *(SokuLib::PlayerMatchData *)(*this)[i - 1].getEndPtr();
	}

	const uint8_t &loadouts(int i) const
	{
		return (*this)[3].getEndPtr()[i];
	}

	const uint8_t &stageId() const
	{
		return (*this)[3].getEndPtr()[4];
	}

	const uint8_t &musicId() const
	{
		return (*this)[3].getEndPtr()[5];
	}

	const uint32_t &randomSeed() const
	{
		return *reinterpret_cast<const uint32_t *>(&(*this)[3].getEndPtr()[6]);
	}

	const uint8_t &matchId() const
	{
		return (*this)[3].getEndPtr()[10];
	}
};

static std::mutex mutex;

#ifdef _DEBUG
std::ofstream logStream;

class ZUtils {
public:
	static constexpr long int CHUNK = {16384};

	static int compress(byte *inBuffer, size_t size, std::vector<byte> &outBuffer, int level)
	{
		int ret, flush;
		unsigned have;
		z_stream strm;
		unsigned char out[CHUNK];

		outBuffer.clear();
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, level);
		if (ret != Z_OK)
			return ret;

		strm.avail_in = size;
		strm.next_in = inBuffer;
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = deflate(&strm, Z_FINISH);    /* anyone error value */
			assert(ret != Z_STREAM_ERROR);
			have = CHUNK - strm.avail_out;
			outBuffer.insert(outBuffer.end(), out, out + have);
		} while (strm.avail_out == 0);
		assert(strm.avail_in == 0);
		assert(ret == Z_STREAM_END);
		deflateEnd(&strm);
		return Z_OK;
	}

	static int decompress(byte *inBuffer, size_t size, std::vector<byte> &outBuffer)
	{
		int ret;
		unsigned have;
		z_stream strm;
		unsigned char out[CHUNK];

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		ret = inflateInit(&strm);
		if (ret != Z_OK)
			return ret;

		strm.avail_in = size;
		strm.next_in = inBuffer;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			outBuffer.insert(outBuffer.end(), out, out + have);
		} while (strm.avail_out == 0);
		assert (ret == Z_STREAM_END);

		inflateEnd(&strm);
		return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
	}

	static std::string zerror(int ret)
	{
		switch (ret) {
		case Z_ERRNO:
			if (ferror(stdin))
				return "Error reading from stdin.";
			else if (ferror(stdout))
				return "Error writing ro stdout.";
		case Z_STREAM_ERROR:
			return "Invalid compression level.";
		case Z_DATA_ERROR:
			return "Empty data, invalid or incomplete.";
		case Z_MEM_ERROR:
			return "No memory.";
		case Z_VERSION_ERROR:
			return "zlib version is incompatible.";
		}
	}
};
#endif

struct Deck {
	std::string name;
	std::array<unsigned short, 20> cards;
};

struct Guide {
	bool active = false;
	SokuLib::DrawUtils::Sprite sprite;
	unsigned char alpha = 0;
};

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
	SokuLib::v2::Player* players[4];
	// 0x38
	bool enabledPlayers[4]; // 01 01 00 00
	// 0x3C
	SokuLib::Vector<SokuLib::v2::Player*> activePlayers;
	// 0x4C
	SokuLib::List<SokuLib::v2::Player*> destroyQueue;
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

static unsigned char versionMask[16] = {
	0xB1, 0x62, 0x56, 0xD4, 0xDF, 0x68, 0x28, 0xA0,
	0x1A, 0xEE, 0x14, 0x55, 0xCF, 0x7E, 0xC3, 0x57
};
static unsigned char (__fastcall *og_advanceFrame)(SokuLib::v2::Player *);
static void (*s_originalDrawGradiantBar)(float param1, float param2, float param3);
static int (SokuLib::SelectServer::*s_originalSelectServerOnProcess)();
static int (SokuLib::SelectServer::*s_originalSelectServerOnRender)();
static int (SokuLib::SelectClient::*s_originalSelectClientOnProcess)();
static int (SokuLib::SelectClient::*s_originalSelectClientOnRender)();
static int (SokuLib::Select::*s_originalSelectOnProcess)();
static int (SokuLib::Select::*s_originalSelectOnRender)();
static int (SokuLib::Title::*s_originalTitleOnProcess)();
static void (SokuLib::BattleManager::*s_originalBattleMgrOnRender)();
static int (SokuLib::ProfileDeckEdit::*s_originalCProfileDeckEdit_OnProcess)();
static int (SokuLib::ProfileDeckEdit::*s_originalCProfileDeckEdit_OnRender)();
static SokuLib::ProfileDeckEdit *(SokuLib::ProfileDeckEdit::*s_originalCProfileDeckEdit_Destructor)(unsigned char param);
static SokuLib::ProfileDeckEdit *(SokuLib::ProfileDeckEdit::*og_CProfileDeckEdit_Init)(int param_2, int param_3, SokuLib::Sprite *param_4);
static void (__fastcall *og_handleInputs)(SokuLib::v2::Player *);
static void (__fastcall *ogBattleMgrHandleCollision)(SokuLib::BattleManager*, int, void*, SokuLib::v2::Player*);
static void (__stdcall *s_origLoadDeckData)(char *, void *, SokuLib::DeckInfo &, int, SokuLib::Deque<short> &);
static void (*og_FUN4098D6)(int, int, int, int);

#define currentIndex(p) (*(char *)((int)p + 0x14E))
#define originalIndex(p) (*(char *)((int)p + 0x14F))

struct CEffectManager {
	void **vtable;
	void **vtable2;
	char offset_0x8[0x90];
};

struct ExtraChrSelectData {
	SokuLib::CDesign::Object *name;
	SokuLib::CDesign::Object *portrait;
	SokuLib::CDesign::Object *name2;
	SokuLib::CDesign::Object *portrait2;
	SokuLib::CDesign::Object *spellCircle;
	SokuLib::CDesign::Object *charObject;
	SokuLib::CDesign::Object *deckObject;
	SokuLib::CDesign::Object *gear;
	SokuLib::CDesign::Sprite *cursor;
	SokuLib::CDesign::Sprite *deckSelect;
	SokuLib::CDesign::Sprite *colorSelect;
	SokuLib::v2::AnimationObject *object;
	SokuLib::InputHandler chrHandler;
	SokuLib::InputHandler palHandler;
	SokuLib::InputHandler deckHandler;
	SokuLib::Sprite portraitSprite;
	SokuLib::Sprite circleSprite;
	SokuLib::Sprite gearSprite;
	CEffectManager effectMgr;
	int baseNameY = 0;
	int portraitTexture = 0;
	int circleTexture = 0;
	int charNameCounter;
	int portraitCounter;
	int portraitCounter2;
	int cursorCounter;
	int deckIndCounter;
	int chrCounter;
	bool needInit;
	bool isInit = false;
};

struct UnderObjects {
	SokuLib::CDesign::Object *underObj;
	SokuLib::CDesign::Object *cardBars[5];
	SokuLib::CDesign::Object *cardGages[5];
	SokuLib::CDesign::Object *cardFaceDown[5];
	SokuLib::CDesign::Object *orbGages[5];
	SokuLib::CDesign::Object *orbFull[5];
	SokuLib::CDesign::Object *orbBars[5];
	SokuLib::CDesign::Object *orbBrokenGage[5];
	SokuLib::CDesign::Object *orbBrokenBar[5];
	SokuLib::CDesign::Object *cardSlots[5];
};

struct ResetValue {
	unsigned int offset;
	unsigned char value;
	unsigned int size;
};

struct ChrInfo {
	bool blockedByWall = false;
	bool canControl = false;
	bool gotHit = false;
	bool started = false;
	bool starting = false;
	bool tagging = false;
	bool deathTag = false;
	bool slowTag = false;
	bool ending = false;
	bool ended = false;
	bool callInit = true;
	unsigned char burstCharges = 0;
	unsigned char activePose = 0;
	unsigned short meterReq = 0;
	unsigned tagAntiBuffer = 0;
	unsigned startup = 0;
	unsigned cutscene = 0;
	unsigned tagTimer = 0;
	unsigned nb = 0;
	unsigned cd = 0;
	unsigned ctr = 0;
	unsigned cost = 0;
	unsigned maxCd = 0;
	unsigned stance = 0;
	unsigned cardName = 0;
	unsigned recovery = 0;
	unsigned startMin = 0;
	unsigned startMax = 0;
	unsigned stanceCtr = 0;
	unsigned startTimer = 0;
	unsigned loadoutIndex = 0;
	unsigned currentStance = 0;
	unsigned meter = 0;
	SokuLib::Action end;
	SokuLib::Action action;
	SokuLib::Character chr;
	SokuLib::KeyInput startKeys;
	SokuLib::KeyInput allowedKeys;
	SokuLib::KeyInput releasedKeys;
	std::optional<unsigned> collisionLimit;
	std::optional<unsigned> startAction;
	std::vector<ResetValue> resetValues;
	SokuLib::Vector2<std::optional<int>> pos;
	SokuLib::Vector2<std::optional<int>> speed;
	SokuLib::Vector2<std::optional<int>> offset;
	SokuLib::Vector2<std::optional<int>> gravity;
	bool (*cond)(SokuLib::v2::Player *mgr, ChrInfo &This) = nullptr;

	ChrInfo &operator=(const ChrInfo &other) {
		this->blockedByWall = other.blockedByWall;
		this->canControl = other.canControl;
		this->gotHit = other.gotHit;
		this->started = other.started;
		this->starting = other.starting;
		this->tagging = other.tagging;
		this->deathTag = other.deathTag;
		this->slowTag = other.slowTag;
		this->ending = other.ending;
		this->ended = other.ended;
		this->callInit = other.callInit;
		this->startup = other.startup;
		this->cutscene = other.cutscene;
		this->tagTimer = other.tagTimer;
		this->tagAntiBuffer = other.tagAntiBuffer;
		this->activePose = other.activePose;
		this->nb = other.nb;
		this->cd = other.cd;
		this->ctr = other.ctr;
		this->cost = other.cost;
		this->maxCd = other.maxCd;
		this->stance = other.stance;
		this->cardName = other.cardName;
		this->recovery = other.recovery;
		this->startMin = other.startMin;
		this->startMax = other.startMax;
		this->stanceCtr = other.stanceCtr;
		this->startTimer = other.startTimer;
		this->loadoutIndex = other.loadoutIndex;
		this->currentStance = other.currentStance;
		this->end = other.end;
		this->action = other.action;
		this->chr = other.chr;
		this->startKeys = other.startKeys;
		this->allowedKeys = other.allowedKeys;
		this->releasedKeys = other.releasedKeys;
		this->collisionLimit = other.collisionLimit;
		this->startAction = other.startAction;
		this->resetValues = other.resetValues;
		this->pos = other.pos;
		this->speed = other.speed;
		this->offset = other.offset;
		this->gravity = other.gravity;
		this->cond = other.cond;
		return *this;
	}
};

struct ChrData {
	std::map<std::string, std::pair<std::optional<ChrInfo>, std::optional<ChrInfo>>> elems;
	SokuLib::Vector2i size;
	SpriteEx sprite;
	std::vector<unsigned short> spells;
	unsigned shownCost;
	bool hasStance;
	bool canFly;
};

static const char *keyConfigDatPath = "data/profile/keyconfig/keyconfig_switch.dat";
static char modFolder[1024];
static char nameBuffer[64];
static char soku2Dir[MAX_PATH];
static std::pair<SokuLib::Character, SokuLib::Character> generatedC = {SokuLib::CHARACTER_RANDOM, SokuLib::CHARACTER_RANDOM};
static SokuLib::DrawUtils::RectangleShape rectangle;
static bool spawned = false;
static bool init = false;
static bool kill = false;
static bool disp = false;
static bool disp2 = false;
static bool disp3 = false;
static bool anim = false;
static bool assetsLoaded = false;
static SokuLib::v2::InfoManager hud2;
static SokuLib::v2::InfoManager *&hud1 = *(SokuLib::v2::InfoManager **)0x8985E8;
static bool hudInit = false;
static UnderObjects hudElems[4];
static ExtraChrSelectData chrSelectExtra[2];
static SokuLib::Character lastChrs[4];
static bool generated = false;
static int selectedDecks[2] = {0, 0};
static int editSelectedProfile = 0;
static int oldHud = 0;
static bool displayCards = true;
static bool profileSelectReady = false;
static bool hasSoku2 = false;
static int counter = 0;
static SokuLib::SWRFont defaultFont;
static SokuLib::SWRFont font;
static std::unique_ptr<std::array<unsigned short, 20>> fakeDeck[4];
static unsigned char errorCounter = 0;
static SokuLib::DrawUtils::Sprite arrowSprite;
static SokuLib::DrawUtils::Sprite baseSprite;
static SokuLib::DrawUtils::Sprite nameSprite;
static SokuLib::DrawUtils::Sprite noSprite;
static SokuLib::DrawUtils::Sprite noSelectedSprite;
static SokuLib::DrawUtils::Sprite yesSprite;
static SokuLib::DrawUtils::Sprite yesSelectedSprite;
static SokuLib::DrawUtils::Sprite cardHolder;
static SokuLib::DrawUtils::Sprite meterIndicator;
static SokuLib::DrawUtils::Sprite cardHiddenSmall;
static SokuLib::DrawUtils::Sprite cardLocked;
static SokuLib::DrawUtils::Sprite bombIcon;
static SokuLib::DrawUtils::Sprite cardBlank[5];
static SokuLib::DrawUtils::Sprite highlight[20];
static SokuLib::DrawUtils::Sprite bigHighlight[20];
static unsigned highlightAnimation[4] = {0, 0, 0, 0};
static Guide createDeckGuide;
static Guide selectDeckGuide;
static Guide editBoxGuide;
static bool copyBoxDisplayed = false;
static bool renameBoxDisplayed = false;
static bool deleteBoxDisplayed = false;
static unsigned editSelectedDeck = 0;
static std::vector<Deck> editedDecks;
static char editingBoxObject[0x164];
static unsigned char copyBoxSelectedItem = 0;
static unsigned char deleteBoxSelectedItem = 0;
static bool saveError = false;
static bool escPressed = false;
static bool forceCardCost = false;
static std::string lastLoadedProfile;
static std::string loadedProfiles_[2];
static std::pair<ChrInfo, ChrInfo> currentChr;
static std::vector<std::array<ChrData, 3>> loadedData{22};
static SokuLib::DrawUtils::Sprite gagesEffects[3];
static unsigned char loadouts[4] = {1, 1, 1, 1};
static SokuLib::InputHandler loadoutHandler[4];
static unsigned chrLoadingIndex = 0;

auto getCharName = (char *(*)(int))0x43F3F0;
auto setRenderMode = [](int mode) {
	((void (__thiscall *)(int, int))0x404B80)(0x896B4C, mode);
};
auto InputHandler_HandleInput = (bool (__thiscall *)(SokuLib::InputHandler &))0x41FBF0;
auto getChrName = (char *(*)(SokuLib::Character))0x43F3F0;
auto getInputManager = (SokuLib::KeymapManager *(*)(int index))0x43E040;
auto getInputManagerIndex = (char (*)(int index))0x43E070;
auto initInputManagerArray = (SokuLib::KeyManager *(*)(int index, bool))0x43E6A0;

static std::array<std::map<unsigned char, std::vector<Deck>>, 5> loadedDecks;
std::map<unsigned char, std::map<unsigned short, SokuLib::DrawUtils::Sprite>> cardsTextures;
std::map<unsigned, std::vector<unsigned short>> characterSpellCards;
std::map<unsigned, std::array<unsigned short, 20>> defaultDecks;
std::map<unsigned, std::string> names{
	{ SokuLib::CHARACTER_REIMU, "reimu" },
	{ SokuLib::CHARACTER_MARISA, "marisa" },
	{ SokuLib::CHARACTER_SAKUYA, "sakuya" },
	{ SokuLib::CHARACTER_ALICE, "alice" },
	{ SokuLib::CHARACTER_PATCHOULI, "patchouli" },
	{ SokuLib::CHARACTER_YOUMU, "youmu" },
	{ SokuLib::CHARACTER_REMILIA, "remilia" },
	{ SokuLib::CHARACTER_YUYUKO, "yuyuko" },
	{ SokuLib::CHARACTER_YUKARI, "yukari" },
	{ SokuLib::CHARACTER_SUIKA, "suika" },
	{ SokuLib::CHARACTER_REISEN, "udonge" },
	{ SokuLib::CHARACTER_AYA, "aya" },
	{ SokuLib::CHARACTER_KOMACHI, "komachi" },
	{ SokuLib::CHARACTER_IKU, "iku" },
	{ SokuLib::CHARACTER_TENSHI, "tenshi" },
	{ SokuLib::CHARACTER_SANAE, "sanae" },
	{ SokuLib::CHARACTER_CIRNO, "chirno" },
	{ SokuLib::CHARACTER_MEILING, "meirin" },
	{ SokuLib::CHARACTER_UTSUHO, "utsuho" },
	{ SokuLib::CHARACTER_SUWAKO, "suwako" },
};
static std::map<unsigned char, unsigned> nbSkills{
	{ SokuLib::CHARACTER_REIMU, 12 },
	{ SokuLib::CHARACTER_MARISA, 12 },
	{ SokuLib::CHARACTER_SAKUYA, 12 },
	{ SokuLib::CHARACTER_ALICE, 12 },
	{ SokuLib::CHARACTER_PATCHOULI, 15 },
	{ SokuLib::CHARACTER_YOUMU, 12 },
	{ SokuLib::CHARACTER_REMILIA, 12 },
	{ SokuLib::CHARACTER_YUYUKO, 12 },
	{ SokuLib::CHARACTER_YUKARI, 12 },
	{ SokuLib::CHARACTER_SUIKA, 12 },
	{ SokuLib::CHARACTER_REISEN, 12 },
	{ SokuLib::CHARACTER_AYA, 12 },
	{ SokuLib::CHARACTER_KOMACHI, 12 },
	{ SokuLib::CHARACTER_IKU, 12 },
	{ SokuLib::CHARACTER_TENSHI, 12 },
	{ SokuLib::CHARACTER_SANAE, 12 },
	{ SokuLib::CHARACTER_CIRNO, 12 },
	{ SokuLib::CHARACTER_MEILING, 12 },
	{ SokuLib::CHARACTER_UTSUHO, 12 },
	{ SokuLib::CHARACTER_SUWAKO, 12 }
};

std::array<SokuLib::Deque<unsigned short>, 4> netplayDeck;
static std::pair<SokuLib::PlayerInfo, SokuLib::PlayerInfo> assists = {
	SokuLib::PlayerInfo{SokuLib::CHARACTER_CIRNO, 0, 0, 0, 0, {}, nullptr},
	SokuLib::PlayerInfo{SokuLib::CHARACTER_MARISA, 1, 0, 0, 0, {}, nullptr}
};
static void (*og_loadDat)(const char *path);
static std::map<unsigned, std::array<std::array<unsigned, 5>, 3>> loadedLoadouts;
static std::mt19937 random;

#ifdef _DEBUG
void displayPacket(SokuLib::Packet *packet, const std::string &start)
{
	mutex.lock();
	logStream << start;
	mutex.unlock();
	switch (packet->type) {
	case SokuLib::CLIENT_GAME:
	case SokuLib::HOST_GAME:
		mutex.lock();
		logStream << "type: " << SokuLib::PacketTypeToString(packet->type);
		mutex.unlock();
		switch (packet->game.event.type) {
		case SokuLib::GAME_MATCH: {
			auto packet_ = (PacketGameMatchEvent *)packet;

			mutex.lock();
			logStream << " p1: " << (*packet_)[0] << std::endl;
			logStream << " p2: " << (*packet_)[1] << std::endl;
			logStream << " p3: " << (*packet_)[2] << std::endl;
			logStream << " p4: " << (*packet_)[3] << std::endl;
			logStream << " loadouts: [";
			logStream << (int) (*packet_).loadouts(0) << ", ";
			logStream << (int) (*packet_).loadouts(1) << ", ";
			logStream << (int) (*packet_).loadouts(2) << ", ";
			logStream << (int) (*packet_).loadouts(1) << "]";
			logStream << ", stageId: " << (int) (*packet_).stageId();
			logStream << ", musicId: " << (int) (*packet_).musicId();
			logStream << ", randomSeed: " << (*packet_).randomSeed();
			logStream << ", matchId: " << (int) (*packet_).matchId();
			mutex.unlock();
			break;
		}
		case SokuLib::GAME_REPLAY: {
			std::vector<byte> out;
			int err = ZUtils::decompress(packet->game.event.replay.compressedData, packet->game.event.replay.replaySize, out);

			logStream << ", compressedSize: " << (int)packet->game.event.replay.replaySize;
			if (err != Z_OK) {
				logStream << ", failed to decompress data: " << ZUtils::zerror(err);
				break;
			}

			struct Test {
				unsigned frameId;
				unsigned maxFrameId;
				unsigned char gameId;
				unsigned char length;
				unsigned short data[1];
			};
			auto a = reinterpret_cast<Test *>(out.data());

			logStream << ", frameId: " << a->frameId;
			logStream << ", maxFrameId: " << a->maxFrameId;
			logStream << ", gameId: " << static_cast<int>(a->gameId);
			logStream << ", inputCount: " << static_cast<int>(a->length);
			logStream << ", inputs: [" << std::hex;
			for (int i = 0; i < a->length; i++)
				logStream << (i == 0 ? "" : ", ") << a->data[i];
			logStream << "]" << std::dec;
			break;
		}
		default:
			mutex.lock();
			displayGameEvent(logStream, packet->game.event);
			mutex.unlock();
			break;
		}
		break;
	default:
		mutex.lock();
		SokuLib::displayPacketContent(logStream, *packet);
		mutex.unlock();
	}
	mutex.lock();
	logStream << std::endl;
	mutex.unlock();
}
#else
void displayPacket(SokuLib::Packet *, const std::string &)
{
}
#endif

unsigned int sokuRand(int max)
{
	return random() % max;
}

void loadExtraDatFiles(const char *path)
{
	og_loadDat(path);
	SokuLib::appendDatFile((std::string(modFolder) + "/assets.dat").c_str());
}

void loadSoku2CSV(LPWSTR path)
{
	std::ifstream stream{path};
	std::string line;

	printf("Loading character CSV from %S\n", path);
	if (stream.fail()) {
		printf("%S: %s\n", path, strerror(errno));
		return;
	}
	while (std::getline(stream, line)) {
		std::stringstream str{line};
		unsigned id;
		std::string idStr;
		std::string codeName;
		std::string shortName;
		std::string fullName;
		std::string skills;

		std::getline(str, idStr, ';');
		std::getline(str, codeName, ';');
		std::getline(str, shortName, ';');
		std::getline(str, fullName, ';');
		std::getline(str, skills, '\n');
		if (str.fail()) {
			printf("Skipping line %s: Stream failed\n", line.c_str());
			continue;
		}
		try {
			id = std::stoi(idStr);
		} catch (...){
			printf("Skipping line %s: Invalid id\n", line.c_str());
			continue;
		}
		names[id] = codeName;
		nbSkills[id] = (std::count(skills.begin(), skills.end(), ',') + 1 - skills.empty()) * 3;
		printf("%s has %i skills\n", codeName.c_str(), nbSkills[id]);
	}
}

void loadSoku2Config()
{
	puts("Looking for Soku2 config...");

	int argc;
	wchar_t app_path[MAX_PATH];
	wchar_t setting_path[MAX_PATH];
	wchar_t **arg_list = CommandLineToArgvW(GetCommandLineW(), &argc);

	wcsncpy(app_path, arg_list[0], MAX_PATH);
	PathRemoveFileSpecW(app_path);
	if (GetEnvironmentVariableW(L"SWRSTOYS", setting_path, sizeof(setting_path)) <= 0) {
		if (arg_list && argc > 1 && StrStrIW(arg_list[1], L"ini")) {
			wcscpy(setting_path, arg_list[1]);
			LocalFree(arg_list);
		} else {
			wcscpy(setting_path, app_path);
			PathAppendW(setting_path, L"\\SWRSToys.ini");
		}
		if (arg_list) {
			LocalFree(arg_list);
		}
	}
	printf("Config file is %S\n", setting_path);

	wchar_t moduleKeys[1024];
	wchar_t moduleValue[MAX_PATH];
	GetPrivateProfileStringW(L"Module", nullptr, nullptr, moduleKeys, sizeof(moduleKeys), setting_path);
	for (wchar_t *key = moduleKeys; *key; key += wcslen(key) + 1) {
		wchar_t module_path[MAX_PATH];

		GetPrivateProfileStringW(L"Module", key, nullptr, moduleValue, sizeof(moduleValue), setting_path);

		wchar_t *filename = wcsrchr(moduleValue, '/');

		printf("Check %S\n", moduleValue);
		if (!filename)
			filename = app_path;
		else
			filename++;
		for (int i = 0; filename[i]; i++)
			filename[i] = tolower(filename[i]);
		if (wcscmp(filename, L"soku2.dll") != 0)
			continue;

		hasSoku2 = true;
		wcscpy(module_path, app_path);
		PathAppendW(module_path, moduleValue);
		while (auto result = wcschr(module_path, '/'))
			*result = '\\';
		printf("Soku2 dll is at %S\n", module_path);
		PathRemoveFileSpecW(module_path);
		printf("Found Soku2 module folder at %S\n", module_path);
		PathAppendW(module_path, L"\\config\\info\\characters.csv");
		loadSoku2CSV(module_path);
		return;
	}
}

void generateClientDecks(SokuLib::NetObject &obj)
{
	obj.playerData[1].cards = netplayDeck[1];
}

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

static void drawCollisionBox(const SokuLib::v2::GameObjectBase &manager)
{
	SokuLib::DrawUtils::FloatRect rect{};
	const SokuLib::Box &box = *manager.boxData.collisionBoxPtr;

	if (!manager.boxData.collisionBoxPtr)
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

static void loadTexture(SokuLib::DrawUtils::Texture &container, const char *path, bool shouldExist = true)
{
	int text = 0;
	SokuLib::Vector2u size;
	int *ret = SokuLib::textureMgr.loadTexture(&text, path, &size.x, &size.y);

	printf("Loading texture %s\n", path);
	if (!ret || !text) {
		puts("Couldn't load texture...");
		if (shouldExist)
			MessageBoxA(SokuLib::window, ("Cannot load game asset " + std::string(path)).c_str(), "Game texture loading failed", MB_ICONWARNING);
	}
	container.setHandle(text, size);
}

static inline void loadTexture(SokuLib::DrawUtils::Sprite &container, const char *path, bool shouldExist = true)
{
	loadTexture(container.texture, path, shouldExist);
	container.setSize(container.texture.getSize());
	container.rect.width = container.getSize().x;
	container.rect.height = container.getSize().y;
	container.rect.top = 0;
	container.rect.left = 0;
}

static void initGuide(Guide &guide)
{
	guide.sprite.setPosition({0, 464});
	guide.sprite.setSize({640, 16});
	guide.sprite.rect.width = guide.sprite.getSize().x;
	guide.sprite.rect.height = guide.sprite.getSize().y;
	guide.sprite.rect.top = 0;
	guide.sprite.rect.left = 0;
}

static void loadCardAssets()
{
	int text;
	char buffer[128];
	SokuLib::DrawUtils::Texture tmp;
	SokuLib::FontDescription desc;

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = FONT_HEIGHT;
	desc.weight = FW_BOLD;
	desc.italic = 0;
	desc.shadow = 2;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = 0;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "Tahoma");
	font.create();
	font.setIndirect(desc);
	strcpy(desc.faceName, SokuLib::defaultFontName);
	desc.weight = FW_REGULAR;
	defaultFont.create();
	defaultFont.setIndirect(desc);

	puts("Loading card assets");
	for (int i = 0; i <= 20; i++) {
		sprintf(buffer, "data/card/common/card%03i.bmp", i);
		loadTexture(cardsTextures[SokuLib::CHARACTER_RANDOM][i], buffer);
	}
	loadTexture(cardsTextures[SokuLib::CHARACTER_RANDOM][21], "data/battle/cardFaceDown.bmp");
	for (auto &elem : names) {
		auto j = elem.first;

		for (int i = nbSkills[j]; i; i--) {
			sprintf(buffer, "data/card/%s/card%03i.bmp", names[j].c_str(), 99 + i);
			loadTexture(cardsTextures[j][99 + i], buffer);
		}
		for (auto &card : characterSpellCards.at(static_cast<SokuLib::Character>(j))) {
			sprintf(buffer, "data/card/%s/card%03i.bmp", names[j].c_str(), card);
			loadTexture(cardsTextures[j][card], buffer);
		}
	}
	loadTexture(baseSprite,            "data/menu/21_Base.bmp");
	loadTexture(nameSprite,            "data/profile/20_Name.bmp");
	loadTexture(arrowSprite,           "data/profile/deck2/sayuu.bmp");
	loadTexture(noSprite,              "data/menu/23a_No.bmp");
	loadTexture(noSelectedSprite,      "data/menu/23b_No.bmp");
	loadTexture(yesSprite,             "data/menu/22a_Yes.bmp");
	loadTexture(yesSelectedSprite,     "data/menu/22b_Yes.bmp");
	loadTexture(editBoxGuide.sprite,   "data/guide/09.bmp");
	loadTexture(createDeckGuide.sprite,"data/guide/createDeck.bmp");
	loadTexture(selectDeckGuide.sprite,"data/guide/selectDeck.bmp");
	loadTexture(cardHolder,            "data/battle/cardGaugeSmallB.bmp");
	loadTexture(meterIndicator,        "data/battle/cardBarSmallB.bmp");
	loadTexture(cardHiddenSmall,       "data/battle/cardFaceDownSmall.bmp");
	loadTexture(cardLocked,            "data/infoeffect/lock_card.bmp");
	loadTexture(bombIcon,              "data/infoeffect/bomb.bmp");
	for (int i = 0; i < 5; i++) {
		sprintf(buffer, "data/infoeffect/blank_%icost.bmp", i + 1);
		loadTexture(cardBlank[i], buffer);
	}
	for (int i = 0; i < 20; i++) {
		sprintf(buffer, "data/infoeffect/cardMax%03i.bmp", i);
		loadTexture(bigHighlight[i], buffer);
	}
	for (int i = 0; i < 20; i++) {
		sprintf(buffer, "data/infoeffect/cardMaxSmall%03i.bmp", i);
		loadTexture(highlight[i], buffer);
	}
	initGuide(createDeckGuide);
	initGuide(selectDeckGuide);
	initGuide(editBoxGuide);
}

static void drawPositionBox(const SokuLib::v2::GameObjectBase &manager)
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

static void drawHurtBoxes(const SokuLib::v2::GameObjectBase &manager)
{
	if (manager.boxData.hurtBoxCount > 5)
		return;

	for (int i = 0; i < manager.boxData.hurtBoxCount; i++)
		drawBox(
			manager.boxData.hurtBoxes[i],
			manager.boxData.hurtBoxesRotation[i],
			SokuLib::Color::Green,
			(manager.boxData.frameData->frameFlags.chOnHit ? SokuLib::Color::Cyan : SokuLib::Color::Green) * BOXES_ALPHA
		);
}

static void drawHitBoxes(const SokuLib::v2::GameObjectBase &manager)
{
	if (manager.boxData.hitBoxCount > 5)
		return;

	for (int i = 0; i < manager.boxData.hitBoxCount; i++)
		drawBox(manager.boxData.hitBoxes[i], manager.boxData.hitBoxesRotation[i], SokuLib::Color::Red, SokuLib::Color::Red * BOXES_ALPHA);
}

static void drawPlayerBoxes(const SokuLib::v2::Player &manager, bool playerBoxes = true)
{
	if (playerBoxes) {
		drawCollisionBox(manager);
		drawHurtBoxes(manager);
		drawHitBoxes(manager);
		drawPositionBox(manager);
	}

	auto &array = manager.objectList->getList();

	for (const auto elem : array) {
		if (elem->isActive && elem->collisionLimit != 0) {
			drawHurtBoxes(*elem);
			drawHitBoxes(*elem);
			drawPositionBox(*elem);
		}
	}
}

void displaySkillLevelUpEffect(SokuLib::v2::Player &mgr)
{
	auto &batlMgr = SokuLib::getBattleMgr();

	mgr.createEffect(71, mgr.position.x, mgr.position.y + 100.f, mgr.direction, 1);
	return;
	if (batlMgr.currentRound >= 2)
		mgr.createEffect(134, mgr.position.x, mgr.position.y, 1, 1);
	else
		mgr.createEffect(131 + batlMgr.currentRound, mgr.position.x, mgr.position.y, 1, 1);
}

void displayTaggingEffect(SokuLib::v2::Player &mgr)
{
	mgr.createEffect(71, mgr.position.x, mgr.position.y + 100.f, 1, 1);
}

void displayTaggingEffect2(SokuLib::v2::Player &mgr)
{
	mgr.createEffect(69, mgr.position.x, mgr.position.y + 100.f, 1, 1);
}

bool condBasic(SokuLib::v2::Player *mgr, ChrInfo &This)
{
	return mgr->hitStop == 0 && ((mgr->frameState.actionId != This.action && mgr->frameState.actionId < SokuLib::ACTION_5A) || mgr->collisionType);
}

bool waitEnd(SokuLib::v2::Player *mgr, ChrInfo &This)
{
	return mgr->hitStop == 0 && mgr->frameState.actionId != This.action && mgr->frameState.actionId < SokuLib::ACTION_5A;
}

bool waitIdle(SokuLib::v2::Player *mgr, ChrInfo &)
{
	return mgr->frameState.actionId == SokuLib::ACTION_IDLE;
}

void updateObject(SokuLib::v2::Player *main, SokuLib::v2::Player *mgr, ChrInfo &chr)
{
	if (chr.tagging) {
		if (main->renderInfos.yRotation != 0) {
			main->renderInfos.yRotation -= 10;
		} else if (chr.deathTag) {
			chr.tagTimer++;
			if (
				main->boxData.hitBoxCount != 0 &&
				(
					main->comboRate != 0 || main->comboCount == 0 ||
					main->gameData.opponent->frameState.actionId < SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN ||
					main->gameData.opponent->frameState.actionId >= SokuLib::ACTION_GRABBED
				)
			) {
				chr.activePose = main->frameState.poseId;
				main->gameData.opponent->checkTurnAround();
				main->gameData.opponent->setAction(SokuLib::ACTION_AIR_HIT_BIG_HITSTUN4);
				main->gameData.opponent->speed.x = -30;
				main->gameData.opponent->speed.y = 0;
				main->damageOpponent(0, 0, main->comboCount == 0, false);
				if (main->gameData.opponent->isGrounded())
					main->gameData.opponent->untech = 2;
				else if (main->gameData.opponent->direction == SokuLib::LEFT)
					main->gameData.opponent->untech = 2 + (1240 - main->gameData.opponent->position.x) / 30;
				else
					main->gameData.opponent->untech = 2 + (main->gameData.opponent->position.x - 40) / 30;
				SokuLib::camera.forceXCenter = false;
				SokuLib::camera.forceYCenter = false;
				SokuLib::camera.forceScale = false;
			}
			if (chr.activePose == 0)
				main->timeStop = 2;
			else if (main->gameData.opponent->frameState.actionId == SokuLib::ACTION_AIR_HIT_BIG_HITSTUN4) {
				if (main->boxData.hitBoxCount == 0)
					main->hitStop = 2;
				if (main->gameData.opponent->position.y < 100 && chr.tagTimer % 6 == 0)
					main->gameData.opponent->createEffect(
						128,
						main->gameData.opponent->position.x - main->gameData.opponent->direction * 50, 0,
						-main->gameData.opponent->direction, 1
					);
			} else if (main->frameState.actionId != SokuLib::ACTION_BOMB)
				chr.tagging = false;
			main->projectileInvulTimer = 2;
			main->grabInvulTimer = 2;
			main->meleeInvulTimer = 2;
		} else if (chr.slowTag && chr.tagTimer < SLOW_TAG_STARTUP) {
			chr.tagTimer++;
		} else {
			chr.tagging = false;
			displayTaggingEffect(*mgr);
		}
	}
	if (mgr->timeStop && chr.cardName) {
		oldHud = *(int *)0x8985E8;
		if (originalIndex(mgr) >= 2)
			*(int *)0x8985E8 = (int)&hud2;
		//forceCardCost = true;
		//mgr->consumeCard(0, chr.cost, 0x3C);
		//mgr->handInfo.hand.push_back({static_cast<unsigned short>(chr.cardName), 1});
		//mgr->handInfo.cardCount++;
		//mgr->consumeCard(mgr->handInfo.cardCount - 1, 1, 60);
		//forceCardCost = false;

		unsigned cost = chr.cost * ASSIST_CARD_METER;

		if (mgr->weatherId == SokuLib::WEATHER_MOUNTAIN_VAPOR && chr.meter < chr.cost * ASSIST_CARD_METER)
			cost = chr.meter;
		else if (mgr->weatherId == SokuLib::WEATHER_CLOUDY)
			cost = max(1, chr.cost - 1) * ASSIST_CARD_METER;
		if (chr.meter <= cost)
			chr.meter = 0;
		else
			chr.meter -= cost;
		chr.meterReq = MAX_METER_REQ;
		*(int *)0x8985E8 = oldHud;
		chr.cost = 0;
		chr.cardName = 0;
	}
	if (mgr->renderInfos.yRotation == 90) {
		if (!mgr->keyManager)
			mgr->setAction(SokuLib::ACTION_IDLE);
		mgr->grabInvulTimer = 2;
		mgr->meleeInvulTimer = 2;
		mgr->projectileInvulTimer = 2;
		if (mgr->frameState.actionId == SokuLib::ACTION_STANDING_UP)
			mgr->setAction(SokuLib::ACTION_IDLE);
		if (
			mgr->frameState.actionId >= SokuLib::ACTION_FORWARD_DASH &&
			mgr->frameState.actionId != SokuLib::ACTION_FLY &&
			mgr->frameState.actionId != SokuLib::ACTION_USING_SC_ID_214
		)
			mgr->setAction(SokuLib::ACTION_IDLE);
		if (SokuLib::mainMode == SokuLib::BATTLE_MODE_PRACTICE) {
			chr.cd = 0;
			chr.meterReq = 0;
			chr.meter = loadedData[chr.chr][chr.loadoutIndex].shownCost * ASSIST_CARD_METER;
		}

		int mul = 1;

		if (main->weatherId == SokuLib::WEATHER_TWILIGHT)
			mul *= 2;
		if (chr.cd < mul)
			chr.cd = 0;
		else
			chr.cd -= mul;
		if (mgr->frameState.actionId < SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN)
			mgr->position = main->position;
		if (loadedData[chr.chr][chr.loadoutIndex].canFly) {
			mgr->position.y += 10;
			mgr->speed.y = 0;
			mgr->gravity.y = 0;
			mgr->airDashCount = 0;
		}
		mgr->spiritRegenDelay = 0;
		mgr->currentSpirit = mgr->maxSpirit;
		mgr->speed = {0, 0};
	} else if (chr.nb != 0 && mgr->renderInfos.yRotation != 0) {
		mgr->renderInfos.yRotation -= 10;
		mgr->checkTurnAround();
		if (chr.speed.x)
			mgr->speed.x = *chr.speed.x;
		if (chr.speed.y)
			mgr->speed.y = *chr.speed.y;
		if (chr.gravity.x)
			mgr->gravity.x = *chr.gravity.x;
		if (chr.gravity.y)
			mgr->gravity.y = *chr.gravity.y;
	} else if (chr.startup) {
		chr.startup--;
		if (chr.currentStance == 2) {
			if (mgr->frameState.actionId != 200 || mgr->frameState.sequenceId != 1)
				mgr->setActionSequence(SokuLib::ACTION_FORWARD_DASH, 1);
			mgr->speed.x = 0;
		}
	} else if (
		mgr->frameState.actionId >= SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN &&
		mgr->frameState.actionId <= SokuLib::ACTION_NEUTRAL_TECH &&
		mgr->frameState.actionId != SokuLib::ACTION_KNOCKED_DOWN_STATIC
	) {
		if (!chr.gotHit) {
			chr.nb = 0;
			chr.started = true;
			chr.starting = false;
			chr.ending = false;
			chr.ended = false;
		}
		chr.recovery = 5;
		chr.gotHit = true;
	} else if (!chr.started) {
		if (chr.startAction) {
			mgr->dashTimer = 0;
			mgr->setAction(*chr.startAction);
			if (chr.speed.x)
				mgr->speed.x = *chr.speed.x;
			if (chr.speed.y)
				mgr->speed.y = *chr.speed.y;
			if (chr.gravity.x)
				mgr->gravity.x = *chr.gravity.x;
			if (chr.gravity.y)
				mgr->gravity.y = *chr.gravity.y;
			chr.starting = true;
		} else if (chr.cutscene != 2 || waitIdle(mgr, chr)){
			chr.nb--;
			mgr->dashTimer = 0;
			if (chr.callInit)
				mgr->setAction(chr.action);
			else
				(mgr->*&SokuLib::v2::AnimationObject::setAction)(chr.action);
			if (chr.collisionLimit)
				mgr->collisionLimit = *chr.collisionLimit;
			if (chr.cutscene == 1)
				displaySkillLevelUpEffect(*mgr);
			if (chr.cutscene == 2)
				displayTaggingEffect2(*mgr);
			if (chr.speed.x)
				mgr->speed.x = *chr.speed.x;
			if (chr.speed.y)
				mgr->speed.y = *chr.speed.y;
			if (chr.gravity.x)
				mgr->gravity.x = *chr.gravity.x;
			if (chr.gravity.y)
				mgr->gravity.y = *chr.gravity.y;
		}
		chr.started = true;
	} else if (chr.starting) {
		bool fine = true;

		for (int i = 0; i < 8; i++) {
			int actual = ((int *) &mgr->keyManager->keymapManager->input)[i];
			int expected = ((int *) &chr.startKeys)[i];

			if (expected == 0)
				continue;
			if (actual < 0 && expected < 0)
				continue;
			if (actual > 0 && expected > 0)
				continue;
			fine = false;
			break;
		}
		chr.startTimer++;
		if (chr.startTimer > chr.startMax || (chr.startTimer >= chr.startMin && !fine)) {
			chr.nb--;
			mgr->dashTimer = 0;
			if (chr.callInit)
				mgr->setAction(chr.action);
			else
				(mgr->*&SokuLib::v2::AnimationObject::setAction)(chr.action);
			if (chr.collisionLimit)
				mgr->collisionLimit = *chr.collisionLimit;
			chr.starting = false;
			chr.ending = false;
			chr.ended = false;
			if (chr.cutscene == 1)
				displaySkillLevelUpEffect(*mgr);
			if (chr.speed.x)
				mgr->speed.x = *chr.speed.x;
			if (chr.speed.y)
				mgr->speed.y = *chr.speed.y;
			if (chr.gravity.x)
				mgr->gravity.x = *chr.gravity.x;
			if (chr.gravity.y)
				mgr->gravity.y = *chr.gravity.y;
		}
	} else if (mgr->hitStop) {
	} else if (chr.ending) {
		if (mgr->frameState.actionId != chr.end && mgr->frameState.actionId < SokuLib::ACTION_5A) {
			chr.ending = false;
			chr.ended = true;
			forceCardCost = false;
			chr.currentStance = chr.stance;
			chr.stanceCtr++;
			if (chr.currentStance == 2) {
				if (mgr->frameState.actionId != 200 || mgr->frameState.sequenceId != 1)
					mgr->setActionSequence(SokuLib::ACTION_FORWARD_DASH, 1);
				mgr->speed.x = 0;
			} else if (chr.currentStance == 3 && mgr->frameState.actionId == SokuLib::ACTION_FLY)
				mgr->speed.x = 0;
			if (chr.recovery)
				chr.recovery--;
			else
				mgr->renderInfos.yRotation += 10;
		}
	} else if (chr.ended) {
		if (mgr->renderInfos.yRotation > 90)
			mgr->renderInfos.yRotation = 90;
		chr.currentStance = chr.stance;
		chr.stanceCtr++;
		if (chr.currentStance == 2) {
			if (mgr->frameState.actionId != 200 || mgr->frameState.sequenceId != 1)
				reinterpret_cast<SokuLib::v2::AnimationObject *>(mgr)->setActionSequence(SokuLib::ACTION_FORWARD_DASH, 1);
			mgr->speed.x = 0;
		} else if (chr.currentStance == 3 && mgr->frameState.actionId == SokuLib::ACTION_FLY)
			mgr->speed.x = 0;
		if (chr.recovery)
			chr.recovery--;
		else
			mgr->renderInfos.yRotation += 10;
	} else if (chr.cond(mgr, chr)) {
		if (chr.nb != 0) {
			chr.nb--;
			mgr->dashTimer = 0;
			if (chr.callInit)
				mgr->setAction(chr.action);
			else
				(mgr->*&SokuLib::v2::AnimationObject::setAction)(chr.action);
			if (chr.collisionLimit)
				mgr->collisionLimit = *chr.collisionLimit;
			if (chr.cutscene == 1)
				displaySkillLevelUpEffect(*mgr);
			else if (chr.cutscene == 2)
				displayTaggingEffect2(*mgr);
		} else if (chr.end) {
			chr.ending = true;
			mgr->dashTimer = 0;
			mgr->setAction(chr.end);
		} else {
			chr.currentStance = chr.stance;
			chr.stanceCtr++;
			if (chr.currentStance == 2) {
				if (mgr->frameState.actionId != 200 || mgr->frameState.sequenceId != 1)
					mgr->setActionSequence(SokuLib::ACTION_FORWARD_DASH, 1);
				mgr->speed.x = 0;
			} else if (chr.currentStance == 3 && mgr->frameState.actionId == SokuLib::ACTION_FLY)
				mgr->speed.x = 0;
			if (chr.recovery)
				chr.recovery--;
			else
				mgr->renderInfos.yRotation += 10;
			chr.ended = true;
		}
	}
}

static void initSkillUpgrade(ChrInfo &chr, SokuLib::Character character, SokuLib::v2::Player &mgr, SokuLib::v2::Player &main, unsigned index)
{
	chr.cutscene = 1;
	chr.nb = 1;
	chr.end = SokuLib::ACTION_IDLE;
	chr.blockedByWall = false;
	chr.gotHit = false;
	chr.started = false;
	chr.starting = false;
	chr.ending = false;
	chr.ended = false;
	chr.callInit = false;
	chr.slowTag = false;
	chr.cd = 0;
	chr.maxCd = 0;
	chr.ctr = 0;
	chr.collisionLimit.reset();
	chr.action = SokuLib::ACTION_SYSTEM_CARD;
	chr.chr = character;
	chr.loadoutIndex = index;
	chr.resetValues.clear();
	chr.pos.x.reset();
	chr.pos.y = 0;
	chr.speed.x = 0;
	chr.speed.y = 0;
	chr.offset.x = 60;
	chr.offset.y.reset();
	chr.gravity.x.reset();
	chr.gravity.y.reset();
	chr.recovery = 0;
	chr.cond = waitEnd;
	mgr.position.x = main.position.x + 60 * mgr.direction;
	mgr.position.y = 0;
	mgr.renderInfos.yRotation = 80;
}

void gainMeter(ChrInfo &chr, unsigned meter)
{
	if (chr.meterReq > meter)
		chr.meterReq -= meter;
	else if (chr.meterReq && chr.meterReq <= meter) {
		chr.meterReq = 0;
		SokuLib::playSEWaveBuffer(SokuLib::SFX_RECOVER_ORB);
	} else {
		chr.meter += meter;
		if (chr.meter > ASSIST_CARD_METER * 5)
			chr.meter = ASSIST_CARD_METER * 5;
	}
}

static void initTagAnim(ChrInfo &chr, SokuLib::Character character, SokuLib::v2::Player &mgr, SokuLib::v2::Player &main, unsigned index, unsigned state)
{
	chr.chr = character;
	chr.end = SokuLib::ACTION_IDLE;
	chr.loadoutIndex = index;
	chr.startAction.reset();
	chr.resetValues.clear();
	chr.speed.x = 0;
	chr.speed.y = 0;
	mgr.speed.x = 0;
	mgr.speed.y = 0;
	chr.pos.x.reset();
	chr.offset.x.reset();
	chr.offset.y.reset();
	chr.gravity.x.reset();
	chr.gravity.y.reset();
	memset(&chr.allowedKeys, 0, sizeof(chr.allowedKeys));
	chr.blockedByWall = false;
	chr.gotHit = false;
	chr.started = false;
	chr.starting = false;
	chr.ending = false;
	chr.ended = false;
	chr.callInit = false;
	chr.slowTag = false;
	chr.cutscene = 2;
	chr.nb = 1;
	chr.ctr = 0;
	chr.cost = 0;
	chr.tagTimer = 0;
	chr.recovery = 0;
	chr.startMin = 0;
	chr.startMax = 0;
	chr.startTimer = 0;
	chr.cond = waitIdle;
	chr.action = SokuLib::ACTION_SYSTEM_CARD;
	if (state == 2 && main.frameState.actionId >= SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN && main.frameState.actionId <= SokuLib::ACTION_NEUTRAL_TECH) {
		chr.recovery = 30;
		chr.slowTag = true;
		mgr.setAction(SokuLib::ACTION_BOMB);
		mgr.position.y = 0;
		mgr.collisionLimit = 1;
		mgr.collisionType = SokuLib::v2::GameObjectBase::COLLISION_TYPE_NONE;
		chr.cd = SLOW_TAG_CD;
		chr.maxCd = SLOW_TAG_CD;
		if (!chr.deathTag) {
			mgr.timeStop = 2;
			if (chr.meterReq == 0 && chr.meter >= SLOW_TAG_COST * ASSIST_CARD_METER)
				chr.meter -= SLOW_TAG_COST * ASSIST_CARD_METER;
			else
				chr.burstCharges--;
			chr.cardName = 1;
		}
	} else {
		if ((mgr.position.x - mgr.gameData.opponent->position.x) * (main.position.x - mgr.gameData.opponent->position.x) < 0 && mgr.gameData.opponent->isOnGround()) {
			chr.cd = CROSS_TAG_CD;
			SokuLib::playSEWaveBuffer(SokuLib::SFX_LHFF_CHARGE);
		} else if (mgr.isGrounded() && !main.isGrounded())
			chr.cd = GROUND_TAG_CD;
		else
			chr.cd = TAG_CD;
		chr.maxCd = chr.cd;
		if (state == 2)
			gainMeter(chr, TAG_CALL_METER * main.meterGainMultiplier);
	}
}

bool initAttack(SokuLib::v2::Player *main, SokuLib::v2::Player *obj, ChrInfo &chr, std::pair<std::optional<ChrInfo>, std::optional<ChrInfo>> &data)
{
	auto &atk = main->position.y == 0 ? data.first : data.second;

	if (atk) {
		unsigned stance = chr.currentStance;

		if (atk->cost == 0);
		else if (chr.meterReq)
			return false;
		else if (obj->weatherId != SokuLib::WEATHER_MOUNTAIN_VAPOR) {
			unsigned cost = atk->cost;

			if (cost > 1 && obj->weatherId == SokuLib::WEATHER_CLOUDY)
				cost--;
			if (chr.meter < cost * ASSIST_CARD_METER)
				return false;
		} else if (chr.meter < ASSIST_CARD_METER && atk->cost)
			return false;
		chr = *atk;
		chr.currentStance = stance;
		obj->renderInfos.yRotation -= 10;
		obj->position = main->position;
		switch (chr.currentStance) {
		case 0:
			obj->setAction(SokuLib::ACTION_IDLE);
			break;
		case 1:
			obj->setAction(SokuLib::ACTION_CROUCHED);
			break;
		case 2:
			obj->setActionSequence(SokuLib::ACTION_FORWARD_DASH, 1);
			break;
		case 3:
			obj->setAction(SokuLib::ACTION_FALLING);
			obj->position.y += 200 * (obj->position.y == 0);
			break;
		}
		chr.callInit = true;
		obj->direction = main->direction;
		obj->speed = main->speed;
		if (chr.speed.x)
			obj->speed.x = *chr.speed.x;
		if (chr.speed.y)
			obj->speed.y = *chr.speed.y;
		if (chr.offset.x)
			obj->position.x += *chr.offset.x * obj->direction;
		if (chr.offset.y)
			obj->position.y += *chr.offset.y;
		if (chr.pos.x)
			obj->position.x = *chr.pos.x;
		if (chr.pos.y)
			obj->position.y = *chr.pos.y;
		if (chr.gravity.x)
			obj->gravity.x = *chr.gravity.x;
		if (chr.gravity.y)
			obj->gravity.y = *chr.gravity.y;
		for (auto &r : chr.resetValues)
			memset(&((char *)obj)[r.offset], r.value, r.size);
		chr.started = false;
		chr.ended = false;
		chr.callInit = true;
		gainMeter(chr, ASSIST_CALL_METER * main->meterGainMultiplier);
	}
	return static_cast<bool>(atk);
}

void assisterAttacks(SokuLib::v2::Player *main, SokuLib::v2::Player *obj, ChrInfo &chr, ChrData &data)
{
	if (SokuLib::getBattleMgr().matchState == 1)
		return;
	if (chr.cd || obj->renderInfos.yRotation != 90)
		return;
	if (main->frameState.actionId >= SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN && main->frameState.actionId <= SokuLib::ACTION_NEUTRAL_TECH)
		return;
	if (!main->keyManager)
		return;
	if (main->keyManager->keymapManager->input.select == 0 || main->keyManager->keymapManager->input.select > 2)
		return;

	std::string buttons[] = {"a", "b", "c", "d", "s"};
	auto elems = &data.elems;

	if (data.hasStance)
		for (auto &buttonName : buttons)
			buttonName += std::to_string(chr.currentStance);
	// Spell
	if (main->keyManager->keymapManager->input.verticalAxis > 0) {
		if (elems->find("5" + buttons[4]) != elems->end())
			initAttack(main, obj, chr, (*elems)["5" + buttons[4]]);
		return;
	}
	// Skill 1
	if (main->keyManager->keymapManager->input.horizontalAxis * main->direction < 0) {
		if (elems->find("8" + buttons[0]) != elems->end())
			initAttack(main, obj, chr, (*elems)["8" + buttons[0]]);
		return;
	}
	// Skill 2
	if (main->keyManager->keymapManager->input.horizontalAxis == 0) {
		if (elems->find("8" + buttons[1]) != elems->end())
			initAttack(main, obj, chr, (*elems)["8" + buttons[1]]);
		return;
	}
	// Skill 3
	if (main->keyManager->keymapManager->input.horizontalAxis * main->direction > 0) {
		if (elems->find("8" + buttons[2]) != elems->end())
			initAttack(main, obj, chr, (*elems)["8" + buttons[2]]);
		return;
	}
}

unsigned short getRandomCard(const std::vector<unsigned short> &list, const std::map<unsigned short, unsigned char> &other)
{
	unsigned short card;

	try {
		do
			card = list[sokuRand(list.size())];
		while (other.at(card) >= 4);
	} catch (std::out_of_range &) {}
	return card;
}

void generateFakeDeck(SokuLib::Character chr, SokuLib::Character lastChr, const std::array<unsigned short, 20> *base, std::unique_ptr<std::array<unsigned short, 20>> &buffer)
{
	if (!base) {
		buffer.reset();
		return;
	}

	unsigned last = 100 + nbSkills[chr];
	std::map<unsigned short, unsigned char> used;
	unsigned char c = 0;
	int index = 0;

	buffer = std::make_unique<std::array<unsigned short, 20>>();
	for (int i = 0; i < 20; i++)
		if ((*base)[i] == 21)
			c++;
		else {
			(*buffer)[index] = (*base)[i];
			if (used.find((*buffer)[index]) == used.end())
				used[(*buffer)[index]] = 1;
			else
				used[(*buffer)[index]]++;
			index++;
		}
	if (!c)
		return;

	std::vector<unsigned short> cards;

	for (int i = 0; i < 21; i++)
		cards.push_back(i);
	for (int i = 100; i < last; i++)
		cards.push_back(i);
	for (auto &card : characterSpellCards[chr])
		cards.push_back(card);

	while (c--) {
		(*buffer)[19 - c] = getRandomCard(cards, used);
		if (used.find((*buffer)[19 - c]) == used.end())
			used[(*buffer)[19 - c]] = 1;
		else
			used[(*buffer)[19 - c]]++;
	}
}

void generateFakeDeck(SokuLib::Character chr, SokuLib::Character lastChr, unsigned id, const std::vector<Deck> &bases, std::unique_ptr<std::array<unsigned short, 20>> &buffer)
{
	std::array<unsigned short, 20> randomDeck{21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21};

	printf("generateFakeDeck(%i, %i, %i)\n", chr, lastChr, id);
	if (lastChr == SokuLib::CHARACTER_RANDOM) {
		if (bases.empty())
			return generateFakeDeck(chr, chr, &defaultDecks[chr], buffer);
		return generateFakeDeck(chr, lastChr, &bases[sokuRand(bases.size())].cards, buffer);
	}
	if (id == bases.size())
		return generateFakeDeck(chr, lastChr, &defaultDecks[chr], buffer);
	if (id == bases.size() + 1)
		return generateFakeDeck(chr, lastChr, nullptr, buffer);
	if (id == bases.size() + 2)
		return generateFakeDeck(chr, lastChr, &randomDeck, buffer);
	if (id >= bases.size() + 3)
		return generateFakeDeck(chr, lastChr, &bases[sokuRand(bases.size())].cards, buffer);
	return generateFakeDeck(chr, lastChr, &bases[id].cards, buffer);
}

void convertDeckToSokuFormat(const std::unique_ptr<std::array<unsigned short, 20>> &tmp, SokuLib::Deque<unsigned short> &buffer)
{
	buffer.clear();
	if (!tmp)
		return;
	for (auto i : *tmp)
		buffer.push_back(i);
}

void generateFakeDecks(SokuLib::Character c1, SokuLib::Character c2)
{
	std::lock_guard<std::mutex> guard{ mutex };

	puts("generateFakeDecks(SokuLib::Character c1, SokuLib::Character c2)");
	if (SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER) {
		printf("Main mode %i != %i\n", SokuLib::mainMode, SokuLib::BATTLE_MODE_VSSERVER);
		return;
	}

	if (!generated || generatedC.first != c1) {
		generateFakeDeck(c1, lastChrs[1], selectedDecks[1], loadedDecks[0][c1], fakeDeck[1]);
		convertDeckToSokuFormat(fakeDeck[1], netplayDeck[1]);
		convertDeckToSokuFormat(fakeDeck[1], SokuLib::rightPlayerInfo.effectiveDeck);
		generateClientDecks(SokuLib::getNetObject());
	}
	if (!generated || generatedC.second != c2) {
		generateFakeDeck(c2, lastChrs[3], assists.second.deck, loadedDecks[0][c2], fakeDeck[3]);
		convertDeckToSokuFormat(fakeDeck[3], netplayDeck[3]);
		convertDeckToSokuFormat(fakeDeck[3], assists.second.effectiveDeck);
	}
	generated = true;
}

void generateFakeDecks()
{
	std::lock_guard<std::mutex> guard{ mutex };

	if (generated)
		return;
	generated = true;

	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER) {
		generateFakeDeck(SokuLib::rightChar, lastChrs[1], selectedDecks[1], loadedDecks[0][SokuLib::rightChar], fakeDeck[1]);
		convertDeckToSokuFormat(fakeDeck[1], netplayDeck[1]);
		convertDeckToSokuFormat(fakeDeck[1], SokuLib::rightPlayerInfo.effectiveDeck);
		generateFakeDeck(assists.second.character, lastChrs[3], assists.second.deck, loadedDecks[0][assists.second.character], fakeDeck[3]);
		convertDeckToSokuFormat(fakeDeck[3], netplayDeck[3]);
		convertDeckToSokuFormat(fakeDeck[3], assists.second.effectiveDeck);
		generatedC = {SokuLib::rightChar, assists.second.character};
	} else if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) {
		generateFakeDeck(SokuLib::leftChar, lastChrs[0], selectedDecks[0], loadedDecks[0][SokuLib::leftChar], fakeDeck[0]);
		convertDeckToSokuFormat(fakeDeck[0], netplayDeck[0]);
		convertDeckToSokuFormat(fakeDeck[0], SokuLib::leftPlayerInfo.effectiveDeck);
		generateFakeDeck(assists.first.character, lastChrs[2], assists.first.deck, loadedDecks[0][assists.first.character], fakeDeck[2]);
		convertDeckToSokuFormat(fakeDeck[2], netplayDeck[2]);
		convertDeckToSokuFormat(fakeDeck[2], assists.first.effectiveDeck);
		generatedC = {SokuLib::leftChar, assists.first.character};
	} else {
		generateFakeDeck(SokuLib::leftChar, lastChrs[0], selectedDecks[0], loadedDecks[0][SokuLib::leftChar], fakeDeck[0]);
		convertDeckToSokuFormat(fakeDeck[0], SokuLib::leftPlayerInfo.effectiveDeck);
		generateFakeDeck(SokuLib::rightChar, lastChrs[1], selectedDecks[1], loadedDecks[1][SokuLib::rightChar], fakeDeck[1]);
		convertDeckToSokuFormat(fakeDeck[1], SokuLib::rightPlayerInfo.effectiveDeck);
		generateFakeDeck(assists.first.character, lastChrs[2], assists.first.deck, loadedDecks[0][assists.first.character], fakeDeck[2]);
		convertDeckToSokuFormat(fakeDeck[2], assists.first.effectiveDeck);
		generateFakeDeck(assists.second.character, lastChrs[3], assists.second.deck, loadedDecks[1][assists.second.character], fakeDeck[3]);
		convertDeckToSokuFormat(fakeDeck[3], assists.second.effectiveDeck);
	}
}

void selectProcessCommon(SokuLib::Select *This, int ret)
{
	// TODO: Support editing profile for P3 and P4
	if (!SokuLib::menuManager.empty() && *SokuLib::getMenuObj<int>() == 0x859820) {
		auto obj = SokuLib::getMenuObj<int>();
		auto selected = obj[0x1A];

		if (selected >= 2 && selected <= 3)
			editSelectedProfile = selected - 2;
		else
			editSelectedProfile = 4;
	}

	if (
		(This->leftKeys && This->leftKeys->input.spellcard == 1) ||
		(This->rightKeys && This->rightKeys->input.spellcard == 1)
	) {
		displayCards = !displayCards;
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (This->leftSelectionStage == 7 && This->rightSelectionStage == 7) {
		if (counter < 60)
			counter++;
	} else {
		counter = 0;
		generated = false;
		generatedC = {SokuLib::CHARACTER_RANDOM, SokuLib::CHARACTER_RANDOM};
	}

	for (int i = 0; i < 2; i++) {
		if ((&This->leftSelectionStage)[i] == 1) {
			auto &decks = loadedDecks[i][(&SokuLib::leftPlayerInfo)[i].character];
			auto input = (&This->leftKeys)[i]->input.horizontalAxis;

			if (input == -1 || (input <= -36 && input % 6 == 0)) {
				SokuLib::playSEWaveBuffer(0x27);
				if (selectedDecks[i] == 0)
					selectedDecks[i] = decks.size() + 3 - decks.empty();
				else
					selectedDecks[i]--;
			} else if (input == 1 || (input >= 36 && input % 6 == 0)) {
				SokuLib::playSEWaveBuffer(0x27);
				if (selectedDecks[i] == decks.size() + 3 - decks.empty())
					selectedDecks[i] = 0;
				else
					selectedDecks[i]++;
			}
		}
	}
	if (ret == SokuLib::SCENE_LOADING) {
		//bool pickedRandom = false;

		//if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER) {
		//	pickedRandom = scene.rightRandomDeck || lastRight == SokuLib::CHARACTER_RANDOM;
		//	scene.rightRandomDeck = false;
		//} else if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) {
		//	pickedRandom = scene.leftRandomDeck  || lastLeft  == SokuLib::CHARACTER_RANDOM;
		//	scene.leftRandomDeck = false;
		//}
		//printf("Picked %srandom deck\n", pickedRandom ? "" : "not ");
		generateFakeDecks();
		return;
	}

	if (This->leftSelectionStage != 7 || This->rightSelectionStage != 7 || counter < 30) {
		if (lastChrs[0] != SokuLib::leftChar)
			selectedDecks[0] = 0;
		lastChrs[0] = SokuLib::leftChar;
		if (lastChrs[1] != SokuLib::rightChar)
			selectedDecks[1] = 0;
		lastChrs[1] = SokuLib::rightChar;
		lastChrs[2] = assists.first.character;
		lastChrs[3] = assists.second.character;
	}
	for (int i = 0; i < 2; i++) {
		auto &dat = chrSelectExtra[i];
		auto &stage = (&This->leftSelectionStage)[i];
		float offset;
		auto &info = (&assists.first)[i];

		if (stage >= 4) {
			if (dat.portraitCounter2)
				dat.portraitCounter2--;
		} else {
			if (dat.portraitCounter2 < 15)
				dat.portraitCounter2++;
		}
		dat.portrait2->x2 = -200 + 200 * (dat.portraitCounter2 * dat.portraitCounter2) / (15.f * 15.f);
		dat.name2->y2 = dat.baseNameY + (-30 - i * 10) + (30 + i * 10) * (dat.portraitCounter2 * dat.portraitCounter2) / (15.f * 15.f);
		if (info.character != SokuLib::CHARACTER_RANDOM && stage >= 4) {
			if (dat.portraitCounter)
				dat.portraitCounter--;
		} else {
			if (dat.portraitCounter < 15)
				dat.portraitCounter++;
		}
		if (dat.cursorCounter)
			dat.cursorCounter--;
		if (stage >= 4) {
			if (dat.charNameCounter)
				dat.charNameCounter--;
		} else {
			if (dat.charNameCounter < 15)
				dat.charNameCounter++;
		}

		if (stage >= 4) {
			if (dat.cursorCounter <= 0)
				dat.cursor->x1 = This->charPortraitStartX + This->charPortraitSliceWidth * dat.chrHandler.pos;
			else
				dat.cursor->x1 -= (dat.cursor->x1 - (This->charPortraitStartX + This->charPortraitSliceWidth * dat.chrHandler.pos)) / 6;
		} else {
			if (dat.cursorCounter <= 0)
				dat.cursor->x1 = 700 * i - 50;
			else
				dat.cursor->x1 -= (dat.cursor->x1 - (700 * i - 50)) / 6;
		}
		if (stage <= 4 || stage >= 7) {
			if (dat.deckIndCounter)
				dat.deckIndCounter--;
		} else {
			if (dat.deckIndCounter < 15)
				dat.deckIndCounter++;
		}
		if (stage <= 4) {
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
			dat.object->update();
	}
}

int __fastcall CSelect_OnProcess(SokuLib::Select *This)
{
	int ret = (This->*s_originalSelectOnProcess)();

	selectProcessCommon(This, ret);
	return ret;
}

int __fastcall CSelectCL_OnProcess(SokuLib::SelectClient *This)
{
	int ret = (This->*s_originalSelectClientOnProcess)();

	selectProcessCommon(&This->base, ret);
	return ret;
}

int __fastcall CSelectSV_OnProcess(SokuLib::SelectServer *This)
{
	int ret = (This->*s_originalSelectServerOnProcess)();

	selectProcessCommon(&This->base, ret);
	return ret;
}

void __fastcall CBattleManager_HandleCollision(SokuLib::BattleManager* This, int unused, void* object, SokuLib::v2::Player* character)
{
	auto players = (SokuLib::v2::Player**)((int)This + 0x0C);

	ogBattleMgrHandleCollision(This, unused, object, players[currentIndex(character) | 2]);
	ogBattleMgrHandleCollision(This, unused, object, players[currentIndex(character) & 1]);
}

static int weirdRand(int key, int delay)
{
	static std::map<int, std::pair<int, int>> elems;
	auto it = elems.find(key);

	if (it == elems.end() || it->second.first == 0) {
		int v = rand();

		elems[key] = {delay, v};
		return v;
	}
	it->second.first--;
	return it->second.second;
}

void renderDeck(SokuLib::Character chr, unsigned select, const std::vector<Deck> &decks, SokuLib::Vector2i pos, const char *overridingName = nullptr)
{
	std::vector<unsigned short> deck;
	std::string name;
	SokuLib::Vector2i base = pos;

	if (select == decks.size()) {
		name = "Default deck";
		deck.resize(20, 21);
		memcpy(deck.data(), defaultDecks[chr].data(), 40);
	} else if (select == decks.size() + 1) {
		name = "No deck";
		deck.resize(0);
	} else if (select == decks.size() + 2) {
		name = "Randomized deck";
		deck.resize(20, 21);
	} else if (select == decks.size() + 3 && !decks.empty())
		return renderDeck(chr, weirdRand((int)&decks, 3) % decks.size(), decks, pos, "Any deck");
	else if (select <= decks.size()) {
		name = decks[select].name;
		deck = {decks[select].cards.begin(), decks[select].cards.end()};
	}

	if (overridingName)
		name = overridingName;

	if (!deck.empty() && displayCards) {
		for (int i = 0; i < 10; i++) {
			SokuLib::DrawUtils::Sprite &sprite = (deck[i] < 100 ? cardsTextures[SokuLib::CHARACTER_RANDOM][deck[i]] : cardsTextures[chr][deck[i]]);

			sprite.setPosition(pos);
			sprite.setSize({10, 16});
			sprite.rect.top = sprite.rect.width = 0;
			sprite.rect.width = sprite.texture.getSize().x;
			sprite.rect.height = sprite.texture.getSize().y;
			sprite.tint = SokuLib::Color::White;
			sprite.draw();
			pos.x += 10;
		}
		pos.x = base.x;
		pos.y += 16;
		for (int i = 0; i < 10; i++) {
			SokuLib::DrawUtils::Sprite &sprite = (deck[i + 10] < 100 ? cardsTextures[SokuLib::CHARACTER_RANDOM][deck[i + 10]] : cardsTextures[chr][deck[i + 10]]);

			sprite.setPosition(pos);
			sprite.setSize({10, 16});
			sprite.rect.top = sprite.rect.width = 0;
			sprite.rect.width = sprite.texture.getSize().x;
			sprite.rect.height = sprite.texture.getSize().y;
			sprite.tint = SokuLib::Color::White;
			sprite.draw();
			pos.x += 10;
		}
	}
	pos.y = base.y + 32;

	SokuLib::DrawUtils::Sprite sprite;
	int text;
	int width = 0;

	if (!SokuLib::textureMgr.createTextTexture(&text, name.c_str(), font, TEXTURE_SIZE, FONT_HEIGHT + 18, &width, nullptr)) {
		puts("C'est vraiment pas de chance");
		return;
	}

	pos.x = base.x + 50 - width / 2;
	sprite.setPosition(pos);
	sprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
	sprite.setSize({TEXTURE_SIZE, FONT_HEIGHT + 18});
	sprite.rect = {0, 0, TEXTURE_SIZE, FONT_HEIGHT + 18};
	sprite.tint = SokuLib::Color::White;
	sprite.draw();
}

void renderLoadout(SokuLib::Character chr, unsigned select, SokuLib::Vector2i pos)
{
	auto it = loadedLoadouts.find(chr);

	if (it == loadedLoadouts.end())
		return;
	for (int i = 0; i < 3; i++) {
		pos.x += 95;
		pos.y += 15;
		for (int j = 3; j >= 0; j--) {
			auto cardId = it->second[i][j];
			SokuLib::DrawUtils::Sprite &sprite = (cardId < 100 ? cardsTextures[SokuLib::CHARACTER_RANDOM][cardId] : cardsTextures[chr][cardId]);

			pos.x -= 20 + 15 * (j == 0);
			if (j == 0)
				pos.y -= 15;
			sprite.setPosition(pos + SokuLib::Vector2i{4, 2});
			if (j == 0)
				sprite.setSize({30, 48});
			else
				sprite.setSize({20, 32});
			sprite.setRotation(-M_PI / 6);
			sprite.rect.top = sprite.rect.left = 0;
			sprite.rect.width = sprite.texture.getSize().x;
			sprite.rect.height = sprite.texture.getSize().y;
			sprite.tint = SokuLib::Color::Black;
			if (i != select)
				sprite.tint.a = 0x80;
			sprite.draw();

			sprite.setPosition(pos);
			sprite.tint = SokuLib::Color::White;
			if (i != select)
				sprite.tint.a = 0x80;
			sprite.draw();
			sprite.setRotation(0);

			if (cardId >= 200) {
				SokuLib::DrawUtils::Sprite &costSprite = cardBlank[it->second[i][4] - 1];

				costSprite.setPosition(pos);
				if (j == 0)
					costSprite.setSize({30, 48});
				else
					costSprite.setSize({20, 32});
				costSprite.setRotation(-M_PI / 6);
				costSprite.rect.top = costSprite.rect.left = 0;
				costSprite.rect.width = costSprite.texture.getSize().x;
				costSprite.rect.height = costSprite.texture.getSize().y;
				costSprite.tint = SokuLib::Color::White;
				if (i != select)
					costSprite.tint.a = 0x80;
				costSprite.draw();
				costSprite.setRotation(0);
			}

		}
		pos.x -= 15;
		pos.y += 40;
	}
}

void selectRenderCommon(SokuLib::Select *This)
{
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER) {
		if (This->rightSelectionStage == 1)
			renderDeck(SokuLib::rightChar, selectedDecks[1], loadedDecks[0][SokuLib::rightChar], {28, 384});
		if (This->rightSelectionStage == 5)
			renderDeck(assists.second.character, assists.second.deck, loadedDecks[0][assists.second.character], {178, 384});
	} else if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) {
		if (This->leftSelectionStage == 1)
			renderDeck(SokuLib::leftChar, selectedDecks[0], loadedDecks[0][SokuLib::leftChar], {28, 98});
		if (This->leftSelectionStage == 5)
			renderDeck(assists.first.character, assists.first.deck, loadedDecks[0][assists.first.character], {178, 98});
	} else {
		if (This->leftSelectionStage == 1)
			renderDeck(SokuLib::leftChar, selectedDecks[0], loadedDecks[0][SokuLib::leftChar], {28, 98});
		if (This->rightSelectionStage == 1)
			renderDeck(SokuLib::rightChar, selectedDecks[1], loadedDecks[1][SokuLib::rightChar], {28, 384});
		if (This->leftSelectionStage == 5)
			renderDeck(assists.first.character, assists.first.deck, loadedDecks[0][assists.first.character], {178, 98});
		if (This->rightSelectionStage == 5)
			renderDeck(assists.second.character, assists.second.deck, loadedDecks[1][assists.second.character], {178, 384});
	}

	if (This->leftSelectionStage == 3)
		renderLoadout(SokuLib::leftChar, loadoutHandler[0].pos, {45, 28});
	if (This->rightSelectionStage == 3)
		renderLoadout(SokuLib::rightChar, loadoutHandler[1].pos, {45, 314});
	if (This->leftSelectionStage == 6)
		renderLoadout(assists.first.character, loadoutHandler[2].pos, {195, 28});
	if (This->rightSelectionStage == 6)
		renderLoadout(assists.second.character, loadoutHandler[3].pos, {195, 314});
}

int __fastcall CSelect_OnRender(SokuLib::Select *This)
{
	auto ret = (This->*s_originalSelectOnRender)();

	selectRenderCommon(This);
	return ret;
}

int __fastcall CSelectCL_OnRender(SokuLib::SelectClient *This)
{
	auto ret = (This->*s_originalSelectClientOnRender)();

	selectRenderCommon(&This->base);
	return ret;
}

int __fastcall CSelectSV_OnRender(SokuLib::SelectServer *This)
{
	auto ret = (This->*s_originalSelectServerOnRender)();

	selectRenderCommon(&This->base);
	return ret;
}

void displayAssistGage(ChrInfo &chr, int x, SokuLib::Vector2i bar, SokuLib::Vector2i cross, bool mirror)
{
	auto &sprite = loadedData[chr.chr][chr.loadoutIndex];
	auto &s = sprite.sprite;

	s.clearTransform();
	for (auto &c : s.transfCoords) {
		c.x += x;
		c.y += ASSIST_BOX_Y - sprite.size.y;
	}
	if (mirror) {
		auto tmp1 = s.transfCoords[0];
		auto tmp2 = s.transfCoords[2];

		s.transfCoords[0] = s.transfCoords[1];
		s.transfCoords[1] = tmp1;
		s.transfCoords[2] = s.transfCoords[3];
		s.transfCoords[3] = tmp2;
	}
	if (chr.cd) {
		(s.*SokuLib::union_cast<void(SpriteEx::*)(float, float, float)>(0x7fb200))(0.3f,0.587f,0.114f);
		if (chr.maxCd != chr.cd) {
			if (mirror)
				bar.x += 78 - 79 * (chr.maxCd - chr.cd) / chr.maxCd;
			gagesEffects[1].setPosition(bar);
			gagesEffects[1].setSize({79 * (chr.maxCd - chr.cd) / chr.maxCd, 4});
			gagesEffects[1].draw();
		}
		if (chr.ctr / 30 % 2 == 0) {
			gagesEffects[2].setPosition(cross);
			gagesEffects[2].draw();
		}
		chr.ctr++;
		chr.ctr += SokuLib::activeWeather == SokuLib::WEATHER_TWILIGHT;
	} else {
		s.render();
		gagesEffects[0].setPosition(bar);
		gagesEffects[0].draw();
	}
}

void __fastcall CBattleManager_OnRender(SokuLib::BattleManager *This)
{
	bool hasRIV = LoadLibraryA("ReplayInputView+") != nullptr;
	bool show = hasRIV ? ((RivControl *)((char *)This + sizeof(*This)))->hitboxes : disp;

	(This->*s_originalBattleMgrOnRender)();
	if (!init)
		return;
	if (disp3)
		for (int j = -2; j < 3; j++) {
			for (int i = 0; i < 4; i++) {
				if (j == 0) {
					if (i >= 2 && disp2) {
						float rot = SokuLib::v2::GameDataManager::instance->players[i]->renderInfos.yRotation;

						SokuLib::v2::GameDataManager::instance->players[i]->renderInfos.yRotation = 0;
						(SokuLib::v2::GameDataManager::instance->players[i]->*SokuLib::union_cast<void (SokuLib::v2::Player::*)()>(0x438d20))();
						SokuLib::v2::GameDataManager::instance->players[i]->renderInfos.yRotation = rot;
					} else
						(SokuLib::v2::GameDataManager::instance->players[i]->*SokuLib::union_cast<void (SokuLib::v2::Player::*)()>(0x438d20))();
				}
				SokuLib::v2::GameDataManager::instance->players[i]->objectList->render1(j);
			}
		}
	setRenderMode(1);
	if (This->matchState < 6) {
		displayAssistGage(currentChr.first,  LEFT_ASSIST_BOX_X,  LEFT_BAR,  LEFT_CROSS,  false);
		displayAssistGage(currentChr.second, RIGHT_ASSIST_BOX_X, RIGHT_BAR, RIGHT_CROSS, true);
		if (show && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER && SokuLib::mainMode != SokuLib::BATTLE_MODE_VSCLIENT) {
			if (!hasRIV) {
				drawPlayerBoxes(*SokuLib::v2::GameDataManager::instance->players[0]);
				drawPlayerBoxes(*SokuLib::v2::GameDataManager::instance->players[1]);
			}
			drawPlayerBoxes(*SokuLib::v2::GameDataManager::instance->players[2], SokuLib::v2::GameDataManager::instance->players[2]->renderInfos.yRotation != 90);
			drawPlayerBoxes(*SokuLib::v2::GameDataManager::instance->players[3], SokuLib::v2::GameDataManager::instance->players[3]->renderInfos.yRotation != 90);
		}
	}
}

const char *battleUpperPath1 = "data/battle/battleUpper1.dat";
const char *effectPath2 = "data/infoEffect/effect2.pat";
const char *battleUpperPath2 = "data/battle/battleUpper2.dat";

void getHudElems(SokuLib::v2::InfoManager &hud, UnderObjects &objects, bool offset)
{
	hud.battleUnder.getById(&objects.underObj, offset + 1);
	for (int i = 0; i < 5; i++) {
		hud.battleUnder.getById(&objects.orbBars[i], 20 + offset * 10 + i);
		hud.battleUnder.getById(&objects.orbFull[i], 40 + offset * 10 + i);
		hud.battleUnder.getById(&objects.orbGages[i], 60 + offset * 10 + i);
		hud.battleUnder.getById(&objects.orbBrokenGage[i], 80 + offset * 10 + i);
		hud.battleUnder.getById(&objects.orbBrokenBar[i], 100 + offset * 10 + i);
		hud.battleUnder.getById(&objects.cardBars[i], 120 + offset * 10 + i);
		hud.battleUnder.getById(&objects.cardFaceDown[i], 140 + offset * 10 + i);
		hud.battleUnder.getById(&objects.cardGages[i], 205 + offset * 10 + i);
		hud.battleUnder.getById(&objects.cardSlots[i], 200 + offset * 10 + i);
	}
}

void initHud()
{
	DWORD old;

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	memset((void *)0x47E9CA, 0x90, 6);
	puts("Construct HUD");
	*(const char **)0x47DEC2 = effectPath2;
	*(const char **)0x47DEE5 = battleUpperPath2;
	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	((void (__thiscall *)(SokuLib::v2::InfoManager *))0x47EAF0)(&hud2);
	*(const char **)0x47DEE5 = battleUpperPath1;
	*(int *)0x47DEC2 = 0x85B430;

	SokuLib::v2::Player** players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0xC);
	auto p1 = players[0];
	auto p2 = players[1];
	auto _p1 = SokuLib::v2::GameDataManager::instance->players[0];
	auto _p2 = SokuLib::v2::GameDataManager::instance->players[1];

	puts("Init HUD");
	players[0] = players[2];
	players[1] = players[3];
	SokuLib::v2::GameDataManager::instance->players[0] = SokuLib::v2::GameDataManager::instance->players[2];
	SokuLib::v2::GameDataManager::instance->players[1] = SokuLib::v2::GameDataManager::instance->players[3];
	*(char *)0x47E29C = 0x14;
	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	((void (__thiscall *)(SokuLib::v2::InfoManager *))0x47E260)(&hud2);
	*(char *)0x47E29C = 0xC;
	players[0] = p1;
	players[1] = p2;
	SokuLib::v2::GameDataManager::instance->players[0] = _p1;
	SokuLib::v2::GameDataManager::instance->players[1] = _p2;
	hud2.p1Portrait->pos.x -= 118;
	hud2.p2Portrait->pos.x -= 118;
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);
	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);

	getHudElems(*hud1, hudElems[0], false);
	getHudElems(*hud1, hudElems[1], true);
	getHudElems(hud2, hudElems[2], false);
	getHudElems(hud2, hudElems[3], true);
}

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

void setBulletOwners()
{
	for (auto &obj : SokuLib::v2::GameDataManager::instance->players[2]->objectList->getList())
		obj->gameData.owner = obj->gameData.ally = SokuLib::v2::GameDataManager::instance->players[0];
	for (auto &obj : SokuLib::v2::GameDataManager::instance->players[3]->objectList->getList())
		obj->gameData.owner = obj->gameData.ally = SokuLib::v2::GameDataManager::instance->players[1];
}

void __declspec(naked) restoreOldHud()
{
	__asm {
		MOV EAX, [oldHud]
		PUSH EBX
		MOV EBX, 0x8985E8
		MOV [EBX], EAX
		POP EBX
		JMP setBulletOwners
	}
}

void __declspec(naked) swapStuff()
{
	__asm {
		CMP EDI, 4
		JGE ret_
		PUSH EBX
		MOV EBX, [ESI + 0x40]
		MOV EBX, [EBX + EDI * 4]
		CMP byte ptr [EBX + 0x14F], 2
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
	ret_:
		RET
	}
}

void __fastcall updateCollisionBoxes(SokuLib::BattleManager *This)
{
	auto fct = (void (__thiscall *)(SokuLib::BattleManager *))0x47B840;
	SokuLib::v2::Player** players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0xC);
	float speeds[4] = {0, 0, 0, 0};

	// P1 - P2
	fct(This);
	speeds[0] += players[0]->additionalSpeed.x;
	speeds[1] += players[1]->additionalSpeed.x;
	// P3 - P2
	players[0] = SokuLib::v2::GameDataManager::instance->players[2];
	if (players[0]->renderInfos.yRotation != 90 && players[0]->hp != 0) {
		fct(This);
		speeds[2] += players[0]->additionalSpeed.x;
		speeds[1] += players[1]->additionalSpeed.x;
	}
	// P3 - P4
	players[1] = SokuLib::v2::GameDataManager::instance->players[3];
	if (players[0]->renderInfos.yRotation != 90 && players[1]->renderInfos.yRotation != 90 && players[0]->hp != 0 && players[1]->hp != 0) {
		fct(This);
		speeds[2] += players[0]->additionalSpeed.x;
		speeds[3] += players[1]->additionalSpeed.x;
	}
	// P1 - P4
	players[0] = SokuLib::v2::GameDataManager::instance->players[0];
	if (players[1]->renderInfos.yRotation != 90 && players[1]->hp != 0) {
		fct(This);
		speeds[0] += players[0]->additionalSpeed.x;
		speeds[3] += players[1]->additionalSpeed.x;
	}
	players[1] = SokuLib::v2::GameDataManager::instance->players[1];
	for (int i = 0; i < 4; i++)
		players[i]->additionalSpeed.x = speeds[i];
}

void loadElemJson(std::optional<ChrInfo> &info, nlohmann::json &json, SokuLib::Character chr, unsigned index)
{
	ChrInfo e;

	e.cd = json["cd"];
	e.stance = json.contains("stance") ? json["stance"].get<int>() : 0;
	memset(&e.releasedKeys, 1, sizeof(e.releasedKeys));
	if (json.contains("startKeys") && json["startKeys"].is_array()) {
		int i = 0;

		for (int v : json["startKeys"]) {
			((int *)&e.startKeys)[i] = v;
			if (i >= 8)
				break;
			i++;
		}
	} else
		memset(&e.startKeys, 0, sizeof(e.startKeys));
	if (json.contains("allowedKeys") && json["allowedKeys"].is_array()) {
		int i = 0;

		for (int v : json["allowedKeys"]) {
			((int *)&e.allowedKeys)[i] = v;
			if (i >= 8)
				break;
			i++;
		}
	} else
		memset(&e.allowedKeys, 0, sizeof(e.allowedKeys));
	e.cost = json.contains("cost") && json["cost"].is_number() ? json["cost"].get<unsigned>() : 0;
	e.startup = json.contains("startup") && json["startup"].is_number() ? json["startup"].get<unsigned>() : 0;
	e.cardName = json.contains("cardName") && json["cardName"].is_number() ? json["cardName"].get<unsigned>() : 0;
	e.recovery = json.contains("recovery") && json["recovery"].is_number() ? json["recovery"].get<unsigned>() : 0;
	e.startMin = json.contains("startMin") && json["startMin"].is_number() ? json["startMin"].get<unsigned>() : 10;
	e.startMax = json.contains("startMax") && json["startMax"].is_number() ? json["startMax"].get<unsigned>() : 30;
	if (json.contains("start") && json["start"].is_number())
		e.startAction = json["start"];
	e.ctr = 0;
	e.maxCd = e.cd;
	if (json.contains("collisionLimit") && json["collisionLimit"].is_number())
		e.collisionLimit = json["collisionLimit"];
	e.nb = json["nb"];
	e.canControl = json.contains("canControl") && json["canControl"].is_boolean() && json["canControl"];
	e.blockedByWall = json.contains("wall") && json["wall"].is_boolean() && json["wall"];
	e.action = json["action"];
	e.end = json.contains("end") && json["end"].is_number() ? json["end"].get<SokuLib::Action>() : SokuLib::ACTION_IDLE;
	e.chr = chr;
	e.loadoutIndex = index;
	for (auto &v : json["reset"]) {
		e.resetValues.push_back({
			v["offset"].is_number() ? v["offset"].get<unsigned>() : std::stoul(v["offset"].get<std::string>(), nullptr, 16),
			static_cast<unsigned char>(v.contains("value") && v["value"].is_number() ? v["value"].get<unsigned>() : 0),
			v.contains("size") && v["size"].is_number() ? v["size"].get<unsigned>() : 1
		});
	}
	e.cond = json.contains("cond") && json["cond"].is_string() && json["cond"] == "basic" ? condBasic : waitEnd;
	if (json.contains("posX") && json["posX"].is_number())
		e.pos.x = json["posX"];
	if (json.contains("posY") && json["posY"].is_number())
		e.pos.y = json["posY"];
	if (json.contains("speedX") && json["speedX"].is_number())
		e.speed.x = json["speedX"];
	if (json.contains("speedY") && json["speedY"].is_number())
		e.speed.y = json["speedY"];
	if (json.contains("gravityX") && json["gravityX"].is_number())
		e.gravity.x = json["gravityX"];
	if (json.contains("gravityY") && json["gravityY"].is_number())
		e.gravity.y = json["gravityY"];
	if (json.contains("offsetX") && json["offsetX"].is_number())
		e.offset.x = json["offsetX"];
	if (json.contains("offsetY") && json["offsetY"].is_number())
		e.offset.y = json["offsetY"];
	info = e;
}

void loadAssistData(int i, int index)
{
	std::string basePath = i > SokuLib::CHARACTER_NAMAZU ? (std::string(soku2Dir) + "/config/tag/") : (std::string(modFolder) + "/assets/");
	const char *chrName = (i == SokuLib::CHARACTER_RANDOM || i == SokuLib::CHARACTER_NAMAZU) ? "Empty" : reinterpret_cast<char *(*)(int)>(0x43f3f0)(i);
	SokuLib::DrawUtils::Texture texture;
	nlohmann::json j;
	std::string path = basePath + chrName + "/config" + std::to_string(loadouts[index]) + ".json";
	std::ifstream stream;

	if (texture.loadFromGame(("data/character/" + std::string(chrName) + "/gage.bmp").c_str()) || texture.loadFromGame("data/character/common/gage.bmp")) {
		loadedData[i][loadouts[index]].sprite.setTexture(texture.releaseHandle(), 0, 0, texture.getSize().x, texture.getSize().y);
		loadedData[i][loadouts[index]].size = texture.getSize().to<int>();
	}
	loadedData[i][loadouts[index]].elems.clear();
	stream.open(path);
	printf("Loading %s\n", path.c_str());
	if (stream.fail()) {
		printf("%s: %s\n", path.c_str(), strerror(errno));
		return;
	}
	try {
		const char *c[] = {
			"4a",  "4b",  "4c",  "4d",  "4s",
			"6a",  "6b",  "6c",  "6d",  "6s",
			"5a",  "5b",  "5c",  "5d",  "5s",
			"2a",  "2b",  "2c",  "2d",  "2s",
			"8a",  "8b",  "8c",  "8d",  "8s",
			"4a0", "4b0", "4c0", "4d0", "4s0",
			"6a0", "6b0", "6c0", "6d0", "6s0",
			"5a0", "5b0", "5c0", "5d0", "5s0",
			"2a0", "2b0", "2c0", "2d0", "2s0",
			"8a0", "8b0", "8c0", "8d0", "8s0",
			"4a1", "4b1", "4c1", "4d1", "4s1",
			"6a1", "6b1", "6c1", "6d1", "6s1",
			"5a1", "5b1", "5c1", "5d1", "5s1",
			"2a1", "2b1", "2c1", "2d1", "2s1",
			"8a1", "8b1", "8c1", "8d1", "8s1",
			"4a2", "4b2", "4c2", "4d2", "4s2",
			"6a2", "6b2", "6c2", "6d2", "6s2",
			"5a2", "5b2", "5c2", "5d2", "5s2",
			"2a2", "2b2", "2c2", "2d2", "2s2",
			"8a2", "8b2", "8c2", "8d2", "8s2",
			"4a3", "4b3", "4c3", "4d3", "4s3",
			"6a3", "6b3", "6c3", "6d3", "6s3",
			"5a3", "5b3", "5c3", "5d3", "5s3",
			"2a3", "2b3", "2c3", "2d3", "2s3",
			"8a3", "8b3", "8c3", "8d3", "8s3"
		};

		stream >> j;
		loadedData[i][loadouts[index]].spells = j.contains("spells") && j["spells"].is_array() ? j["spells"].get<std::vector<unsigned short>>() : std::vector<unsigned short>{};
		loadedData[i][loadouts[index]].shownCost = j.contains("shownCost") && j["shownCost"].is_number() ? j["shownCost"].get<unsigned>() : 0;
		loadedData[i][loadouts[index]].canFly = j.contains("canFly") && j["canFly"].is_boolean() && j["canFly"].get<bool>();
		loadedData[i][loadouts[index]].hasStance = j.contains("hasStance") && j["hasStance"].is_boolean() && j["hasStance"].get<bool>();
		for (auto &a : j.items()) {
			bool b = false;

			for (auto s : c)
				if (strcmp(s, a.key().c_str()) == 0) {
					b = true;
					break;
				}
			if (!b) {
				printf("Ignored %s\n", a.key().c_str());
				continue;
			}

			auto &elem = loadedData[i][loadouts[index]].elems[a.key()];
			auto &val = a.value();

			if (val.contains("ground") && val["ground"].is_object())
				loadElemJson(elem.first, val["ground"], static_cast<SokuLib::Character>(i), loadouts[index]);
			if (val.contains("air") && val["air"].is_object())
				loadElemJson(elem.second, val["air"], static_cast<SokuLib::Character>(i), loadouts[index]);
		}
	} catch (std::exception &e) {
		puts(e.what());
		MessageBox(SokuLib::window, e.what(), "Loading error", MB_ICONERROR);
	}
}

void __stdcall loadDeckData(char *charName, void *csvFile, SokuLib::DeckInfo &deck, int param4, SokuLib::Deque<short> &newDeck)
{
	unsigned chr = chrLoadingIndex == 0 ? SokuLib::leftChar : (chrLoadingIndex == 1 ? SokuLib::rightChar : (&assists.first)[chrLoadingIndex - 2].character);
	auto spells = loadedData[chr][loadouts[chrLoadingIndex]].spells;

	if (spells.empty()) {
		printf("No fake deck for %s:%i (Index %i, Id id)\n", charName, loadouts[chrLoadingIndex], chrLoadingIndex, chr);
		chrLoadingIndex++;
		return s_origLoadDeckData(charName, csvFile, deck, param4, newDeck);
	}

	auto size = newDeck.size();
	auto cards = new short[size];

	printf("Creating a fake deck for %s:%i (Index %i, Id id)\n", charName, loadouts[chrLoadingIndex], chrLoadingIndex, chr);
	chrLoadingIndex++;
	for (int i = 0; i < size; i++)
		cards[i] = newDeck[i];
	newDeck.clear();
	for (auto &id : spells)
		newDeck.push_back(id);
	for (int i = 0; newDeck.size() != 20; i++)
		newDeck.push_back(i);
	s_origLoadDeckData(charName, csvFile, deck, param4, newDeck);
	newDeck.clear();
	for (int i = 0; i < size; i++)
		newDeck.push_back(cards[i]);
	delete[] cards;
	s_origLoadDeckData(charName, csvFile, deck, param4, newDeck);
}

extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16])
{
	return true;
}

void __fastcall handlePlayerInputs(SokuLib::v2::Player *This)
{
	og_handleInputs(This);
	if (SokuLib::v2::GameDataManager::instance->players[2])
		og_handleInputs(SokuLib::v2::GameDataManager::instance->players[2]);
	if (SokuLib::v2::GameDataManager::instance->players[3])
		og_handleInputs(SokuLib::v2::GameDataManager::instance->players[3]);
}

void __fastcall onDeath(SokuLib::v2::Player *This)
{
	auto players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0x0C);
	SokuLib::v2::Player *p1 = players[0];
	SokuLib::v2::Player *p2 = players[2];

	if (This != p1 && This != p2) {
		p1 = players[1];
		p2 = players[3];
	}
	p1->roundsWins = p1->hp == 0 && p2->hp == 0;
	p2->roundsWins = p1->roundsWins;
}

unsigned char __fastcall checkWakeUp(SokuLib::v2::Player *This)
{
	auto ret = og_advanceFrame(This);

	if (This->hp == 0)
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

const double yPos = 62;
static bool hudRenderSave[92];
char hudRenderCodeSave[38];

void initHudRender(SokuLib::v2::InfoManager *hud, UnderObjects *p1)
{
	auto arr = (SokuLib::CDesign::Object **)p1;

	for (int i = 0; i < 92; i++)
		hudRenderSave[i] = arr[i]->active;
	memcpy(hudRenderCodeSave, (void *)0x47DBE7, 38);

	if (currentIndex(hud->playerHUD[0].player) != 0) {
		for (int i = 0; i < 46; i++)
			arr[i]->active = false;
		memset((void *)0x47DBE7, 0x90, 19);
	} else {
		for (int i = 0; i < 5; i++) {
			auto size = hud->playerHUD[0].player->deckInfo.queue.size() + hud->playerHUD[0].player->handInfo.hand.size();

			p1[0].cardSlots[i]->active = size > i;
			p1[0].cardGages[i]->active = size > i;
		}
		p1[0].underObj->active = true;
		hud->playerHUD[0].lifebarRed->gauge.heightRatio = 1;
		hud->playerHUD[0].lifebarRed->gauge.widthRatio = 1;
	}
	if (currentIndex(hud->playerHUD[1].player) != 1) {
		for (int i = 46; i < 92; i++)
			arr[i]->active = false;
		memset((void *)0x47DBFA, 0x90, 19);
	} else {
		for (int i = 0; i < 5; i++) {
			auto size = hud->playerHUD[1].player->deckInfo.queue.size() + hud->playerHUD[1].player->handInfo.hand.size();

			p1[1].cardSlots[i]->active = size > i;
			p1[1].cardGages[i]->active = size > i;
		}
		p1[1].underObj->active = true;
	}
}

void restoreHudRender(SokuLib::v2::InfoManager *hud, UnderObjects *p1)
{
	auto arr = (SokuLib::CDesign::Object **)p1;

	for (int i = 0; i < 92; i++)
		arr[i]->active = hudRenderSave[i];
	memcpy((void *)0x47DBE7, hudRenderCodeSave, 38);
}

#define sidedSetPos(side, t, xpos, ypos) (t).setPosition({(side) ? 640 - (xpos) - (int)(t).getSize().x : (xpos), ypos})

void displayGrayedOut(const SokuLib::DrawUtils::Sprite &sprite)
{
	DWORD o;

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &o);
	auto old = SokuLib::TamperNearJmpOpr(0x7FB269, &SokuLib::DrawUtils::Sprite::draw);
	auto fun = (void (__thiscall *)(const void *, float, float, float))0x7FB200;

	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	fun(&sprite, 0.299, 0.587, 0.114);
	SokuLib::TamperNearJmpOpr(0x7FB269, old);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, o, &o);
	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
}

void displayCard(SokuLib::v2::Player *mgr, unsigned shown, const ChrInfo &info, bool side, unsigned cardId)
{
	unsigned cost = shown;

	shown -= mgr->weatherId == SokuLib::WEATHER_CLOUDY;
	for (int j = 0; j < 5; j++) {
		sidedSetPos(side, cardHolder, 4 + j * 22, 70);
		cardHolder.draw();
	}
	if (mgr->weatherId != SokuLib::WEATHER_MOUNTAIN_VAPOR) {
		int j = 0;
		float ratio = (info.meter % ASSIST_CARD_METER) / (float)ASSIST_CARD_METER;

		while (j < info.meter / ASSIST_CARD_METER && j < 5) {
			auto &texture = cardBlank[j];

			texture.setSize({18, 28});
			sidedSetPos(side, texture, 6 + j * 22, 72);
			texture.tint = SokuLib::Color::White;
			texture.draw();
			j++;
		}
		if (info.meter % ASSIST_CARD_METER) {
			meterIndicator.rect.height = meterIndicator.texture.getSize().y * ratio;
			meterIndicator.rect.top = meterIndicator.texture.getSize().y - meterIndicator.rect.height;
			meterIndicator.setSize({18, static_cast<unsigned int>(28 * ratio)});
			sidedSetPos(side, meterIndicator, 6 + j * 22, static_cast<int>(72 + 28 - meterIndicator.getSize().y));
			meterIndicator.draw();
		}
		if (shown * ASSIST_CARD_METER <= info.meter && !info.meterReq) {
			setRenderMode(2);
			for (int k = 0; k < shown; k++) {
				sidedSetPos(side, highlight[highlightAnimation[2 + side]], k * 22 - 1, 53);
				highlight[highlightAnimation[2 + side]].draw();
			}
			setRenderMode(1);
		}
	} else for (int j = 0; j < 5; j++) {
		sidedSetPos(side, cardHiddenSmall, 6 + j * 22, 72);
		cardHiddenSmall.draw();
	}
	if (info.meterReq) {
		cardLocked.setSize(cardHiddenSmall.getSize());
		for (int j = 0; j < 5; j++) {
			sidedSetPos(side, cardLocked, 6 + j * 22, 72);
			cardLocked.draw();
		}
	}

	auto chr = cardId < 100 ? SokuLib::CHARACTER_RANDOM : mgr->characterIndex;
	auto &texture = cardsTextures[chr][cardId];
	auto &texture2 = cardBlank[cost - 1];


	texture.setSize({20, 30});
	texture.setRotation(M_PI / 6 * (side ? 1 : -1), {10, 15});
	sidedSetPos(side, texture, 9 + 5 * 22, 72);
	texture.setPosition({
		texture.getPosition().x + (side ? 1 : 2),
		texture.getPosition().y + (side ? 2 : 1)
	});
	texture.tint = SokuLib::Color::Black;
	texture.draw();
	sidedSetPos(side, texture, 9 + 5 * 22, 72);
	texture.tint = SokuLib::Color::White;
	displayGrayedOut(texture);

	texture2.setSize({20, 30});
	texture2.setRotation(M_PI / 6 * (side ? 1 : -1), {10, 15});
	sidedSetPos(side, texture2, 9 + 5 * 22, 72);
	texture2.tint = SokuLib::Color::White;
	displayGrayedOut(texture2);

	cardLocked.setSize({20, 30U});
	cardLocked.setRotation(M_PI / 6 * (side ? 1 : -1));
	sidedSetPos(side, cardLocked, 9 + 5 * 22, 72);
	cardLocked.draw();
	cardLocked.setRotation(0);


	texture.rect.height = texture.texture.getSize().y * (MAX_METER_REQ - info.meterReq) / MAX_METER_REQ;
	texture.setSize({20, 30U * (MAX_METER_REQ - info.meterReq) / MAX_METER_REQ});
	sidedSetPos(side, texture, 9 + 5 * 22, 72);
	texture.draw();
	texture.rect.height = texture.texture.getSize().y;

	texture2.rect.height = texture2.texture.getSize().y * (MAX_METER_REQ - info.meterReq) / MAX_METER_REQ;
	texture2.setSize({20, 30U * (MAX_METER_REQ - info.meterReq) / MAX_METER_REQ});
	sidedSetPos(side, texture2, 9 + 5 * 22, 72);
	texture2.draw();
	texture2.rect.height = texture2.texture.getSize().y;


	for (int i = 0; i < info.burstCharges; i++) {
		sidedSetPos(side, bombIcon, 9 + 5 * 22 + 30 + 18 * i, 76);
		bombIcon.draw();
	}


	texture.setRotation(0);
	texture2.setRotation(0);
}

int __fastcall onHudRender(SokuLib::v2::InfoManager *This)
{
	DWORD old;

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	if (SokuLib::getBattleMgr().matchState <= 5) {
		hud2.p1Portrait->render(0, 0);
		hud2.p2Portrait->render(640, 0);

		// JMP 0047DB95
		*(char *)0x47D857 = 0xE9;
		*(unsigned *)0x47D858 = 0x339;
		initHudRender(&hud2, &hudElems[2]);
		::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
		ogHudRender(&hud2);
		restoreHudRender(&hud2, &hudElems[2]);
		// JNE 0047D9EE
		*(char *)0x47D857 = 0x0F;
		*(char *)0x47D858 = 0x85;
		*(unsigned *)0x47D859 = 0x119;
		::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	}

	initHudRender(This, &hudElems[0]);
	int ret = ogHudRender(This);
	restoreHudRender(This, &hudElems[0]);

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);
	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	if (SokuLib::getBattleMgr().matchState <= 5) {
		hud2.battleUpper.render(0, 0, 26);
	}

	const SokuLib::Vector2i pos1[] = {
		{0, 379},
		{52, 409},
		{72, 412},
		{93, 417},
		{114, 421}
	};
	const SokuLib::Vector2i pos2[] = {
		{575, 379},
		{556, 409},
		{535, 412},
		{514, 417},
		{494, 421}
	};

	for (int i = 0; i < 2; i++) {
		auto player = SokuLib::v2::GameDataManager::instance->players[i];
		auto pos = i ? pos2 : pos1;

		if (player->handInfo.hand.empty())
			continue;

		unsigned cost = max(1, player->handInfo.hand[0].cost - (player->weatherId == SokuLib::WEATHER_CLOUDY));

		if (
			player->handInfo.hand.size() >= cost &&
			player->weatherId != SokuLib::WEATHER_MOUNTAIN_VAPOR &&
			player->handInfo.hand[0].cost
		) {
			setRenderMode(2);
			bigHighlight[highlightAnimation[i]].setPosition(pos[0]);
			bigHighlight[highlightAnimation[i]].draw();
			for (int j = 1; j < cost; j++) {
				highlight[highlightAnimation[i]].setPosition(pos[j]);
				highlight[highlightAnimation[i]].draw();
			}
			setRenderMode(1);
		}
	}

	auto &leftData = loadedData[currentChr.first.chr][currentChr.first.loadoutIndex];
	auto &rightData = loadedData[currentChr.second.chr][currentChr.second.loadoutIndex];

	displayCard(SokuLib::v2::GameDataManager::instance->players[2], leftData.shownCost, currentChr.first, false, leftData.spells.front());
	displayCard(SokuLib::v2::GameDataManager::instance->players[3], rightData.shownCost, currentChr.second, true, rightData.spells.front());
	return ret;
}

bool __fastcall shouldPreventCardCost(SokuLib::v2::Player *player)
{
	if (currentIndex(player) < 2)
		return false;
	if (forceCardCost)
		return false;
	if ((&currentChr.first)[currentIndex(player) - 2].cutscene == 2)
		return false;
	return true;
}

void __declspec(naked) preventCardCost()
{
	__asm {
		PUSH ECX
		CALL shouldPreventCardCost
		POP ECX
		TEST AL, AL
		JZ cont
		RET 0xC
	cont:
		SUB ESP, 0x1C
		PUSH EBX
		PUSH EBP
		MOV EAX, 0x469C75
		JMP EAX
	}
}

// TODO: Cards highlight
void __declspec(naked) updateOtherHud()
{
	__asm {
		LEA ECX, [hud2]
		MOV EAX, 0x47D6F0
		JMP EAX
	}
}

static int skipHealthRegen = 0x47B092;
static int dontSkipHealthRegen = 0x47B083;
static int extraHealing[2];

void __declspec(naked) setHealthRegen()
{
	__asm {
		MOVSX EAX, byte ptr [ECX + 0x14E]
		MOVSX ECX, BX
		MOV dword ptr [ESP + 0x20], ECX
		CMP AL, 2
		JLE dontSkip
		DEC EAX
		DEC EAX
		LEA EAX, [extraHealing + EAX * 4]
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
		MOVSX DI, word ptr [EBX + 0x186]
		MOVSX CX, word ptr [EBX + 0x184]
		ADD ECX, dword ptr [extraHealing]
		CMP CX, DI
		JL lowerThanMax1
		MOV CX, DI
	lowerThanMax1:
		CMP CX, 0x0
		JG higherThanZero1
		MOV ECX, 0
	higherThanZero1:
		MOV [EBX + 0x184], CX

		MOV EBX, [EAX + 0x18]
		MOVSX DI, word ptr [EBX + 0x186]
		MOVSX CX, word ptr [EBX + 0x184]
		ADD ECX, dword ptr [extraHealing + 4]
		CMP CX, DI
		JL lowerThanMax2
		MOV CX, DI
	lowerThanMax2:
		CMP CX, 0x0
		JG higherThanZero2
		MOV ECX, 0
	higherThanZero2:
		MOV [EBX + 0x184], CX
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
static SokuLib::SelectClient *(__fastcall *og_SelectClientConstruct)(SokuLib::SelectClient *This);
static SokuLib::SelectServer *(__fastcall *og_SelectServerConstruct)(SokuLib::SelectServer *This);

void selectConstructCommon(SokuLib::Select *This)
{
	int ret = 0;
	SokuLib::Vector2i size;
	char buffer[128];

	for (int i = 0; i < 4; i++) {
		loadoutHandler[i].pos = loadouts[i];
		loadoutHandler[i].posCopy = loadouts[i];
		loadoutHandler[i].axis = nullptr;
		loadoutHandler[i].maxValue = 3;
	}
	for (int i = 0; i < 2; i++) {
		auto &dat = chrSelectExtra[i];
		auto &profileInfo = (&assists.first)[i];

		if (dat.isInit) {
			((void (__thiscall *)(CEffectManager  *))0x4241B0)(&dat.effectMgr);
			SokuLib::textureMgr.deallocate(dat.portraitTexture);
			SokuLib::textureMgr.deallocate(dat.circleTexture);
		}
		This->designBase3.getById(&dat.name2,       100 + i);
		This->designBase3.getById(&dat.name,        100 + i + 2);
		This->designBase3.getById(&dat.portrait2,   900 + i);
		This->designBase3.getById(&dat.portrait,    900 + i + 2);
		This->designBase3.getById(&dat.spellCircle, 800 + i + 2);
		This->designBase3.getById(&dat.charObject,  700 + i + 2);
		This->designBase3.getById(&dat.deckObject,  720 + i + 2);
		This->designBase3.getById(&dat.gear,        600 + i + 2);
		This->designBase3.getById(&dat.cursor,      400 + i + 2);
		This->designBase3.getById(&dat.deckSelect,  320 + i + 2);
		This->designBase3.getById(&dat.colorSelect, 310 + i + 2);
		This->leftDeckInput.maxValue = 4;
		This->rightDeckInput.maxValue = 4;
		dat.baseNameY = dat.name2->y2;
		dat.cursor->active = true;
		dat.deckSelect->active = true;
		dat.colorSelect->active = true;
		// data/scene/select/character/06b_wheel%dp.bmp
		sprintf(buffer, (char *)0x857C18, i + 1);
		if (!SokuLib::textureMgr.loadTexture(&ret, buffer, &size.x, &size.y))
			printf("Failed to load %s\n", buffer);
		dat.gearSprite.setTexture(ret, 0, 0, size.x, size.y, size.x / 2, size.y / 2);
		ret = 0;

		dat.needInit = true;
		dat.object = nullptr;
		dat.chrHandler.maxValue = 20;
		dat.chrHandler.pos = 0;
		while ((*(unsigned **)&This->offset_0x018[0x84])[dat.chrHandler.pos] != profileInfo.character)
			dat.chrHandler.pos++;
		dat.chrHandler.posCopy = dat.chrHandler.pos;
		dat.palHandler.maxValue = 8;
		dat.palHandler.pos = 0;
		dat.palHandler.posCopy = 0;
		dat.deckHandler.maxValue = 4;
		dat.deckHandler.pos = 0;
		dat.deckHandler.posCopy = 0;
		dat.portraitCounter = 15;
		dat.portraitCounter2 = 15;
		dat.charNameCounter = 15;
		dat.chrCounter = 15;
		dat.cursorCounter = 60;
		dat.cursor->x1 = 700 * i - 50;
		dat.isInit = true;
		((void (__thiscall *)(CEffectManager  *))0x422CF0)(&dat.effectMgr);
		dat.palHandler.pos = profileInfo.palette;
		dat.deckHandler.pos = profileInfo.deck;
		// data/scene/select/character/09b_character/character_%02d.bmp
		sprintf(buffer, (char *)0x85785C, profileInfo.character);
		if (!SokuLib::textureMgr.loadTexture(&ret, buffer, &size.x, &size.y))
			printf("Failed to load %s\n", buffer);
		dat.portraitSprite.setTexture2(ret, 0, 0, size.x, size.y);
		dat.portraitTexture = ret;
	}
	loadedLoadouts.clear();
	for (auto &chr : characterSpellCards) {
		if (chr.first == SokuLib::CHARACTER_RANDOM || chr.first == SokuLib::CHARACTER_NAMAZU)
			continue;

		std::string basePath = chr.first > SokuLib::CHARACTER_NAMAZU ? (std::string(soku2Dir) + "/config/tag/") : (std::string(modFolder) + "/assets/");
		const char *chrName = reinterpret_cast<char *(*)(int)>(0x43f3f0)(chr.first);
		nlohmann::json j;
		std::string path = basePath + chrName + "/loadouts.json";
		std::ifstream stream{path};

		try {
			if (!stream)
				throw std::invalid_argument(path + ": " + strerror(errno));
			stream >> j;
			loadedLoadouts[chr.first] = j;
		} catch (std::exception &e) {
			printf("Failed to load loadouts for %s: %s\n", chrName, e.what());
		}
	}
}

SokuLib::Select *__fastcall CSelect_construct(SokuLib::Select *This)
{
	og_SelectConstruct(This);
	selectConstructCommon(This);
	return This;
}

SokuLib::SelectClient *__fastcall CSelectCL_construct(SokuLib::SelectClient *This)
{
	og_SelectClientConstruct(This);
	selectConstructCommon(&This->base);
	return This;
}

SokuLib::SelectServer *__fastcall CSelectSV_construct(SokuLib::SelectServer *This)
{
	og_SelectServerConstruct(This);
	selectConstructCommon(&This->base);
	return This;
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
	dat.portraitSprite.render(offset * 640 + dat.portrait->x2 + dat.portrait->x1, dat.portrait->y2 + dat.portrait->y1);
}

void __fastcall renderChrSelectChrDataGear(int index)
{
	auto &dat = chrSelectExtra[index];

	dat.gearSprite.rotation = dat.cursor->x1 * 2.5;
	dat.gearSprite.setColor(0xFF000000);
	dat.gearSprite.render(dat.cursor->x1 + dat.gear->x2 + 2, dat.gear->y2);
	dat.gearSprite.setColor(0xFFFFFFFF);
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

	This->charNameSprites[info.character == SokuLib::CHARACTER_RANDOM ? 20 : info.character].render(offset * 300 + dat.name->x2, dat.name->y2);
	if (dat.object && info.character != SokuLib::CHARACTER_RANDOM) {
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

void __fastcall renderChrSelectProfile(int index)
{
//	profiles[index + 2]->sprite.render(chrSelectExtra[index].profileBack->x2 + 88, chrSelectExtra[index].profileBack->y2 + 10);
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

void __declspec(naked) renderChrSelectProfile_hook()
{
	__asm {
		MOV ECX, EDI
		PUSH EAX
		PUSH ESI
		PUSH EDI
		PUSH EBX
		CALL renderChrSelectProfile
		POP EBX
		POP EDI
		POP ESI
		POP EAX
		INC EDI
		ADD ESI, 4
		RET
	}
}

void __fastcall changePalette(SokuLib::Select *This, int index, char palette, bool dir)
{
	bool ok = false;
	auto &mine = (index < 2 ? (&SokuLib::leftPlayerInfo)[index] : (&assists.first)[index - 2]);
	auto &mine2 = (index == 0 ? This->leftPalInput : index == 1 ? This->rightPalInput : chrSelectExtra[index - 2].palHandler);

	while (!ok) {
		ok = true;
		for (int i = 0; i < 2; i++) {
			if (index == i)
				continue;
			if ((&SokuLib::leftPlayerInfo)[i].character == mine.character && (&SokuLib::leftPlayerInfo)[i].palette == palette && (&This->leftSelectionStage)[i] != 0) {
				palette = (palette + 7 - dir * 6) % 8;
				ok = false;
			}
		}
		for (int i = 0; i < 2; i++) {
			if (index == i + 2)
				continue;
			if ((&assists.first)[i].character == mine.character && (&assists.first)[i].palette == palette && (&This->leftSelectionStage)[i] > 4) {
				palette = (palette + 7 - dir * 6) % 8;
				ok = false;
			}
		}
	}
	mine.palette = palette;
	mine2.pos = palette;
	mine2.posCopy = palette;
}

void __declspec(naked) changePalette_hook0()
{
	__asm {
		PUSH EDX
		MOV EAX, [EDX]
		MOV ECX, ESI
		MOV EDX, 1
		PUSH EDX
		PUSH EAX
		MOV EDX, EDI
		SHR EDX, 5
		CALL changePalette
		POP EDX
		RET
	}
}

void __declspec(naked) changePalette_hook1()
{
	__asm {
		PUSH EDX
		MOV EAX, [EDX]
		MOV ECX, ESI
		MOV EDX, [EBX]
		MOV EDX, [EDX + 0x3C]
		CMP EDX, 0
		JGE under
		XOR EDX, EDX
		JMP END
	under:
		MOV EDX, 1
	end:
		PUSH EDX
		PUSH EAX
		MOV EDX, EDI
		SHR EDX, 5
		CALL changePalette
		POP EDX
		RET
	}
}

int updateCharacterSelect_hook_failAddr = 0x42091D;
int updateCharacterSelect_hook_retAddr = 0x4208F4;

void __fastcall chrSelectLastStep(SokuLib::v2::AnimationObject &obj, SokuLib::KeymapManager &inputHandler, int index, char &state)
{
	if (state != 3)
		return;
	if (inputHandler.input.a == 1) {
		chrSelectExtra[index].cursorCounter = 60;
		SokuLib::playSEWaveBuffer(0x28);
		state = 4;
		obj.setAction(1);
		loadouts[index] = loadoutHandler[index].pos;
		return;
	}
	if (inputHandler.input.d == 1) {
		chrSelectExtra[index].cursorCounter = 60;
		SokuLib::playSEWaveBuffer(0x28);
		state = 4;
		obj.setAction(1);
		loadouts[index] = sokuRand(3);
		return;
	}
	if (inputHandler.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		state = 0;
		return;
	}
	loadoutHandler[index].axis = &inputHandler.input.verticalAxis;
	if (InputHandler_HandleInput(loadoutHandler[index]))
		SokuLib::playSEWaveBuffer(0x27);
}

void __fastcall updateCharacterSelect2(SokuLib::Select *This, unsigned i)
{
	char buffer[128];
	SokuLib::Vector2i size;
	auto &dat = chrSelectExtra[i];
	auto &state = (&This->leftSelectionStage)[i];
	auto *input = (&This->leftKeys)[i];
	auto &info = (&assists.first)[i];

	This->leftDeckInput.maxValue = 4;
	This->rightDeckInput.maxValue = 4;
	if (!input)
		return;
	//if (dat.chrHandler.pos == (&This->leftCharInput)[i].pos)
	//	dat.chrHandler.pos = (dat.chrHandler.pos + 1) % dat.chrHandler.maxValue;
	switch (state) {
	case 4:
		dat.chrHandler.axis = &input->input.horizontalAxis;
		if (InputHandler_HandleInput(dat.chrHandler)) {
			if (dat.chrHandler.pos == (&This->leftCharInput)[i].pos) {
				if (input->input.horizontalAxis > 0)
					dat.chrHandler.pos = (dat.chrHandler.pos + 1) % dat.chrHandler.maxValue;
				else if (dat.chrHandler.pos == 0)
					dat.chrHandler.pos = dat.chrHandler.maxValue - 1;
				else
					dat.chrHandler.pos--;
			}
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
		if (input->input.a == 1) {
			char *name = getCharName(info.character);

			SokuLib::playSEWaveBuffer(0x28);
			state = 5;
			changePalette(This, i + 2, info.palette, true);
			sprintf(buffer, (char *)0x8578EC, info.character);
			SokuLib::textureMgr.deallocate(dat.circleTexture);
			if (!SokuLib::textureMgr.loadTexture(&dat.circleTexture, buffer, &size.x, &size.y))
				printf("Failed to load %s\n", buffer);
			dat.circleSprite.setTexture(dat.circleTexture, 0, 0, size.x, size.y, size.x / 2, size.y / 2);
			((void (__thiscall *)(CEffectManager *))dat.effectMgr.vtable[2])(&dat.effectMgr);
			// data/character/%s/palette%d.bmp
			sprintf(buffer, (char *)0x8578C8, name, info.palette);
			((void (__thiscall *)(int, char *, int, int))0x408BE0)(0x8A0048, buffer, 0x896B88, 0x10);
			// data/scene/select/character/%s/stand.xml
			sprintf(buffer, (char *)0x85789C, name);
			((void (__thiscall *)(CEffectManager *, char *, int))dat.effectMgr.vtable[1])(&dat.effectMgr, buffer, 0);
			dat.object = ((SokuLib::v2::AnimationObject *(__thiscall *)(CEffectManager *, int, float, float, int, int, int))dat.effectMgr.vtable[3])(&dat.effectMgr, 0, dat.charObject->x2, dat.charObject->y2, 1, 0, 0);
			dat.object->setPose(0);
			if (lastChrs[2 + i] != info.character)
				dat.deckHandler.pos = 0;
			selectedDecks[2 + i] = dat.deckHandler.pos;
			info.deck = dat.deckHandler.pos;
			dat.deckHandler.maxValue = loadedDecks[i][info.character].size() + 3;
		} else if (input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			state = 0;
			dat.cursorCounter = 60;
			if ((&SokuLib::leftPlayerInfo)[i].character == SokuLib::CHARACTER_RANDOM) {
				(&SokuLib::leftPlayerInfo)[i].character = *((SokuLib::Character *(__thiscall *)(const void *, int))0x420380)(&This->offset_0x018[0x80], (&This->leftCharInput)[i].pos);
				((void (__thiscall *)(void *, int, int))0x41FD80)(This, i, (&SokuLib::leftPlayerInfo)[i].character);
			}
		} else if (input->input.d == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			state = 7;
			info.character = SokuLib::CHARACTER_RANDOM;
			dat.charNameCounter = 15;
		}
		break;
	case 5:
		dat.palHandler.axis = &input->input.verticalAxis;
		dat.deckHandler.axis = &input->input.horizontalAxis;
		if (InputHandler_HandleInput(dat.deckHandler)) {
			SokuLib::playSEWaveBuffer(0x27);
			selectedDecks[2 + i] = dat.deckHandler.pos;
			info.deck = dat.deckHandler.pos;
		}
		if (InputHandler_HandleInput(dat.palHandler)) {
			char *name = getCharName(info.character);
			int poseId = dat.object->frameState.poseId;
			int poseFrame = dat.object->frameState.poseFrame;

			changePalette(This, i + 2, dat.palHandler.pos, *dat.palHandler.axis > 0);
			SokuLib::playSEWaveBuffer(0x27);
			((void (__thiscall *)(CEffectManager *))dat.effectMgr.vtable[2])(&dat.effectMgr);
			// data/character/%s/palette%d.bmp
			sprintf(buffer, (char *)0x8578C8, name, info.palette);
			((void (__thiscall *)(int, char *, int, int))0x408BE0)(0x8A0048, buffer, 0x896B88, 0x10);
			// data/scene/select/character/%s/stand.xml
			sprintf(buffer, (char *)0x85789C, name);
			((void (__thiscall *)(CEffectManager *, char *, int))dat.effectMgr.vtable[1])(&dat.effectMgr, buffer, 0);
			dat.object = ((SokuLib::v2::AnimationObject *(__thiscall *)(CEffectManager *, int, float, float, int, int, int))dat.effectMgr.vtable[3])(&dat.effectMgr, 0, dat.charObject->x2, dat.charObject->y2, 1, 0, 0);
			dat.object->setPose(poseId);
			dat.object->frameState.poseFrame = poseFrame;
		}
		if (input->input.a == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			state = 6;
		} else if (input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			state = 4;
		}
		break;
	case 6:
		if (input->input.a == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			state++;
			dat.object->setAction(1);
			loadouts[i + 2] = loadoutHandler[i + 2].pos;
			break;
		}
		if (input->input.d == 1) {
			SokuLib::playSEWaveBuffer(0x28);
			state = 7;
			dat.object->setAction(1);
			loadouts[i + 2] = sokuRand(3);
			break;
		}
		if (input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			state = 4;
			break;
		}
		loadoutHandler[i + 2].axis = &input->input.verticalAxis;
		if (InputHandler_HandleInput(loadoutHandler[i + 2]))
			SokuLib::playSEWaveBuffer(0x27);
		break;
	case 7:
		if (input->input.b == 1) {
			SokuLib::playSEWaveBuffer(0x29);
			state = 4;
			if (info.character == SokuLib::CHARACTER_RANDOM) {
				info.character = *((SokuLib::Character *(__thiscall *)(const void *, int))0x420380)(&This->offset_0x018[0x80], dat.chrHandler.pos);
				dat.charNameCounter = 15;
			}
		}
	}
}

void onChrSelectComplete()
{
	if (assists.first.character == SokuLib::CHARACTER_RANDOM)
		assists.first.character = static_cast<SokuLib::Character>(sokuRand(20));
	if (assists.second.character == SokuLib::CHARACTER_RANDOM)
		assists.second.character = static_cast<SokuLib::Character>(sokuRand(20));
}

void __fastcall onStageSelectCancel(SokuLib::Select *This, int index)
{
	if ((&assists.first)[index].character == SokuLib::CHARACTER_RANDOM)
		(&assists.first)[index].character = (*(SokuLib::Character **)&This->offset_0x018[0x84])[chrSelectExtra[index].chrHandler.pos];
}

void __declspec(naked) onStageSelectCancel_hook()
{
	__asm {
		MOV EDX, EBP
		MOV ECX, ESI
		JMP onStageSelectCancel
	}
}

int __fastcall CTitle_OnProcess(SokuLib::Title *This)
{
	if (SokuLib::menuManager.empty() || *SokuLib::getMenuObj<int>() != SokuLib::ADDR_VTBL_DECK_CONSTRUCTION_CHR_SELECT_MENU)
		editSelectedProfile = 4;
	assists.first.character = SokuLib::CHARACTER_CIRNO;
	assists.first.palette = 0;
	assists.first.deck = 0;
	assists.second.character = SokuLib::CHARACTER_MARISA;
	assists.second.palette = 0;
	assists.second.deck = 0;
	memset(loadouts, 1, sizeof(loadouts));
	return (This->*s_originalTitleOnProcess)();
}

void updateGuide(Guide &guide)
{
	if (guide.active && guide.alpha != 255)
		guide.alpha += 15;
	else if (!guide.active && guide.alpha)
		guide.alpha -= 15;
}

void renderGuide(Guide &guide)
{
	guide.sprite.tint.a = guide.alpha;
	guide.sprite.draw();
}

bool saveDeckFromGame(SokuLib::ProfileDeckEdit *This, std::array<unsigned short, 20> &deck)
{
	unsigned index = 0;

	for (auto &pair : *This->editedDeck) {
		auto i = pair.second;

		while (i) {
			if (index == 20)
				return false;
			deck[index] = pair.first;
			i--;
			index++;
		}
	}
	while (index < 20) {
		deck[index] = 21;
		index++;
	}
	std::sort(deck.begin(), deck.end());
	return true;
}

void loadDeckToGame(SokuLib::ProfileDeckEdit *This, const std::array<unsigned short, 20> &deck)
{
	int count = 0;

	This->editedDeck->clear();
	for (int i = 0; i < 20; i++)
		if (deck[i] != 21) {
			auto iter = This->editedDeck->find(deck[i]);
			if (iter == This->editedDeck->end())
				(*This->editedDeck)[deck[i]] = 1;
			else
				iter->second++;
			count++;
		}
	This->displayedNumberOfCards = count;
}

void __fastcall CProfileDeckEdit_SwitchCurrentDeck(SokuLib::ProfileDeckEdit *This, int, int deckID)
{
	auto FUN_0044f930 = SokuLib::union_cast<void (SokuLib::ProfileDeckEdit::*)(char param_1)>(0x44F930);

	This->selectedDeck = 0;
	if (!saveDeckFromGame(This, editedDecks[editSelectedDeck].cards)) {
		errorCounter = 120;
		return;
	}
	if (deckID == 1) {
		if (editSelectedDeck == editedDecks.size() - 1)
			editSelectedDeck = 0;
		else
			editSelectedDeck++;
	} else {
		if (editSelectedDeck == 0)
			editSelectedDeck = editedDecks.size() - 1;
		else
			editSelectedDeck--;
	}
	loadDeckToGame(This, editedDecks[editSelectedDeck].cards);
	(This->*FUN_0044f930)('\0');
}

void renameBoxRender()
{
	int text;
	int width;
	SokuLib::DrawUtils::Sprite textSprite;
	SokuLib::DrawUtils::RectangleShape rect;

	nameSprite.setPosition({160, 192});
	nameSprite.setSize(nameSprite.texture.getSize());
	nameSprite.rect = {
		0, 0,
		static_cast<int>(nameSprite.texture.getSize().x),
		static_cast<int>(nameSprite.texture.getSize().y)
	};
	nameSprite.tint = SokuLib::Color::White;
	nameSprite.draw();

	if (!SokuLib::textureMgr.createTextTexture(&text, nameBuffer, font, TEXTURE_SIZE, 18, &width, nullptr)) {
		puts("C'est vraiment pas de chance");
		return;
	}
	auto render = (int(__thiscall*) (void*, float, float))0x42a050;

	render(editingBoxObject, 276, 217);
}

void openRenameBox(SokuLib::ProfileDeckEdit *This)
{
	auto setup_global = (int(__thiscall*) (void*, bool))0x40ea10;
	auto init_fun = (void(__thiscall*) (void*, int, int))0x429e70;

	SokuLib::playSEWaveBuffer(0x28);
	if (editSelectedDeck == editedDecks.size() - 1) {
		editedDecks.back().name = "Deck #" + std::to_string(editedDecks.size());
		editedDecks.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	}

	//:magic_wand:
	init_fun(editingBoxObject, 0x89A4F8, 24);
	setup_global((void*)0x8A02F0, true);
	strncpy((char *)0x8A02F8, editedDecks[editSelectedDeck].name.c_str(), 24);
	renameBoxDisplayed = true;
}

void renameBoxUpdate(SokuLib::KeyManager *keys)
{
	auto update = (char (__thiscall*) (void*))0x429F00;

	renameBoxDisplayed = *(bool *)0x8A0CFF;
	if (!renameBoxDisplayed)
		SokuLib::playSEWaveBuffer(0x29);
	else if (update(editingBoxObject) == 1) {
		SokuLib::playSEWaveBuffer(0x28);
		editedDecks[editSelectedDeck].name = (char *)0x8A02F8;
		renameBoxDisplayed = false;
	}
}

void deleteBoxRender()
{
	int text;
	SokuLib::DrawUtils::Sprite textSprite;
	SokuLib::DrawUtils::Sprite &yes = deleteBoxSelectedItem ? yesSelectedSprite : yesSprite;
	SokuLib::DrawUtils::Sprite &no  = deleteBoxSelectedItem ? noSprite : noSelectedSprite;

	if (deleteBoxSelectedItem == 2)
		return;

	baseSprite.setPosition({160, 192});
	baseSprite.setSize(baseSprite.texture.getSize());
	baseSprite.rect = {
		0, 0,
		static_cast<int>(baseSprite.texture.getSize().x),
		static_cast<int>(baseSprite.texture.getSize().y)
	};
	baseSprite.tint = SokuLib::Color::White;
	baseSprite.draw();

	yes.setPosition({242, 228});
	yes.setSize(yes.texture.getSize());
	yes.rect = {
		0, 0,
		static_cast<int>(yes.texture.getSize().x),
		static_cast<int>(yes.texture.getSize().y)
	};
	yes.tint = SokuLib::Color::White;
	yes.draw();

	no.setPosition({338, 228});
	no.setSize(no.texture.getSize());
	no.rect = {
		0, 0,
		static_cast<int>(no.texture.getSize().x),
		static_cast<int>(no.texture.getSize().y)
	};
	no.tint = SokuLib::Color::White;
	no.draw();

	if (!SokuLib::textureMgr.createTextTexture(&text, ("Delete deck " + editedDecks[editSelectedDeck].name + " ?").c_str(), defaultFont, TEXTURE_SIZE, FONT_HEIGHT + 18, nullptr, nullptr)) {
		puts("C'est vraiment pas de chance");
		return;
	}
	textSprite.setPosition({164, 202});
	textSprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.setSize({TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.rect = {
		0, 0, TEXTURE_SIZE, FONT_HEIGHT + 18
	};
	textSprite.tint = SokuLib::Color::White;
	textSprite.fillColors[2] = textSprite.fillColors[3] = SokuLib::Color::Blue;
	textSprite.draw();
}

void openDeleteBox()
{
	SokuLib::playSEWaveBuffer(0x28);
	deleteBoxDisplayed = true;
	deleteBoxSelectedItem = 0;
}

void deleteBoxUpdate(SokuLib::KeyManager *keys)
{
	auto horizontal = abs(keys->keymapManager->input.horizontalAxis);

	SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>()->guideVector[2].active = true;
	if (deleteBoxSelectedItem == 2) {
		if (!keys->keymapManager->input.a)
			deleteBoxDisplayed = false;
		return;
	}
	if (keys->keymapManager->input.b || SokuLib::checkKeyOneshot(1, false, false, false)) {
		SokuLib::playSEWaveBuffer(0x29);
		deleteBoxDisplayed = false;
	}
	if (horizontal == 1 || (horizontal >= 36 && horizontal % 6 == 0)) {
		deleteBoxSelectedItem = !deleteBoxSelectedItem;
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (keys->keymapManager->input.a == 1) {
		if (deleteBoxSelectedItem) {
			editedDecks.erase(editedDecks.begin() + editSelectedDeck);
			loadDeckToGame(SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>(), editedDecks[editSelectedDeck].cards);
			SokuLib::playSEWaveBuffer(0x28);
		} else
			SokuLib::playSEWaveBuffer(0x29);
		deleteBoxSelectedItem = 2;
	}
}

void copyBoxRender()
{
	int text;
	int width;
	SokuLib::DrawUtils::Sprite textSprite;

	baseSprite.setPosition({160, 192});
	baseSprite.setSize(baseSprite.texture.getSize());
	baseSprite.rect = {
		0, 0,
		static_cast<int>(baseSprite.texture.getSize().x),
		static_cast<int>(baseSprite.texture.getSize().y)
	};
	baseSprite.tint = SokuLib::Color::White;
	baseSprite.draw();

	const std::string &name = copyBoxSelectedItem == editedDecks.size() - 1 ? "Default deck" : editedDecks[copyBoxSelectedItem].name;

	if (!SokuLib::textureMgr.createTextTexture(&text, name.c_str(), font, TEXTURE_SIZE, FONT_HEIGHT + 18, &width, nullptr)) {
		puts("C'est vraiment pas de chance");
		return;
	}

	constexpr float increase = 1;
	SokuLib::Vector2i pos{static_cast<int>(321 - (width / 2) * increase), 230};

	textSprite.setPosition(pos);
	textSprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.setSize({static_cast<unsigned>(TEXTURE_SIZE * increase), static_cast<unsigned>((FONT_HEIGHT + 18) * increase)});
	textSprite.rect = {
		0, 0, TEXTURE_SIZE, FONT_HEIGHT + 18
	};
	textSprite.tint = SokuLib::Color::White;
	textSprite.draw();

	pos.x -= 32 * increase;
	pos.y -= 6 * increase;
	arrowSprite.rect = {0, 0, 32, 32};
	arrowSprite.setPosition(pos);
	arrowSprite.setSize({static_cast<unsigned>(32 * increase + 1), static_cast<unsigned>(32 * increase + 1)});
	arrowSprite.tint = SokuLib::Color::White;
	arrowSprite.draw();

	pos.x += 32 * increase + width * increase;
	arrowSprite.rect.left = 32;
	arrowSprite.setPosition(pos);
	arrowSprite.draw();

	if (!SokuLib::textureMgr.createTextTexture(&text, "Choose a deck to copy", defaultFont, TEXTURE_SIZE, FONT_HEIGHT + 18, &width, nullptr)) {
		puts("C'est vraiment pas de chance");
		return;
	}
	textSprite.setPosition({166, 200});
	textSprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.fillColors[2] = textSprite.fillColors[3] = SokuLib::Color::Blue;
	textSprite.draw();
}

void openCopyBox(SokuLib::ProfileDeckEdit *This)
{
	SokuLib::playSEWaveBuffer(0x28);
	copyBoxDisplayed = true;
	copyBoxSelectedItem = 0;
}

void copyBoxUpdate(SokuLib::KeyManager *keys)
{
	if (keys->keymapManager->input.b || SokuLib::checkKeyOneshot(1, false, false, false)) {
		SokuLib::playSEWaveBuffer(0x29);
		copyBoxDisplayed = false;
	}

	auto horizontal = abs(keys->keymapManager->input.horizontalAxis);

	SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>()->guideVector[2].active = true;
	if (horizontal == 1 || (horizontal >= 36 && horizontal % 6 == 0)) {
		if (keys->keymapManager->input.horizontalAxis < 0) {
			if (copyBoxSelectedItem == 0)
				copyBoxSelectedItem = editedDecks.size() - 1;
			else
				copyBoxSelectedItem--;
		} else {
			if (copyBoxSelectedItem == editedDecks.size() - 1)
				copyBoxSelectedItem = 0;
			else
				copyBoxSelectedItem++;
		}
		SokuLib::playSEWaveBuffer(0x27);
	}
	if (keys->keymapManager->input.a == 1) {
		auto menu = SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>();

		editedDecks.back().name = "Deck #" + std::to_string(editedDecks.size());
		loadDeckToGame(SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>(), copyBoxSelectedItem == editedDecks.size() - 1 ? defaultDecks[menu->editedCharacter] : editedDecks[copyBoxSelectedItem].cards);
		editedDecks.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
		copyBoxDisplayed = false;
		openRenameBox(menu);
	}
}

static void loadDefaultDecks()
{
	char buffer[] = "data/csv/000000000000/deck.csv";

	for (auto [id, name] : names) {
		sprintf(buffer, "data/csv/%s/deck.csv", name.c_str());
		printf("Loading default deck %s\n", buffer);

		SokuLib::CSVParser parser{buffer};
		std::array<unsigned short, 20> deck;
		auto &cards = characterSpellCards[id];

		for (int i = 0; i < 20; i++) {
			auto str = parser.getNextCell();

			try {
				auto card = std::stoul(str);

				if (card > 20 && std::find(cards.begin(), cards.end(), card) == cards.end())
					MessageBoxA(
						SokuLib::window,
						("Warning: Default deck for " + name + " contains invalid card " + str).c_str(),
						"Default deck invalid",
						MB_ICONWARNING
					);
				deck[i] = card;
			} catch (std::exception &e) {
				MessageBoxA(
					SokuLib::window,
					(
						"Fatal error: Cannot load default deck for " + name + ":\n" +
						"In file " + buffer + ": Cannot parse cell #" + std::to_string(i + 1) +
						" \"" + str + "\": " + e.what()
					).c_str(),
					"Loading default deck failed",
					MB_ICONERROR
				);
				abort();
			}
		}
		std::sort(deck.begin(), deck.end());
		defaultDecks[id] = deck;
	}
}

static void loadAllExistingCards()
{
	char buffer[] = "data/csv/000000000000/spellcard.csv";

	for (auto [id, name] : names) {
		sprintf(buffer, "data/csv/%s/spellcard.csv", name.c_str());
		printf("Loading cards from %s\n", buffer);

		SokuLib::CSVParser parser{buffer};
		std::vector<unsigned short> valid;
		int i = 0;

		do {
			auto str = parser.getNextCell();

			i++;
			try {
				valid.push_back(std::stoul(str));
			} catch (std::exception &e) {
				MessageBoxA(
					SokuLib::window,
					(
						"Fatal error: Cannot load cards list for " + name + ":\n" +
						"In file " + buffer + ": Cannot parse cell #1 at line #" + std::to_string(i) +
						" \"" + str + "\": " + e.what()
					).c_str(),
					"Loading default deck failed",
					MB_ICONERROR
				);
				abort();
			}
		} while (parser.goToNextLine());
		characterSpellCards[id] = valid;
	}
}

static void initAssets()
{
	if (assetsLoaded)
		return;
	assetsLoaded = true;
	loadAllExistingCards();
	loadDefaultDecks();
	loadCardAssets();

	gagesEffects[0].texture.loadFromGame("data/character/sanae/gageBa000.cv0");
	gagesEffects[0].setSize(gagesEffects[0].texture.getSize());
	gagesEffects[0].rect.width = gagesEffects[0].texture.getSize().x;
	gagesEffects[0].rect.height = gagesEffects[0].texture.getSize().y;

	gagesEffects[1].texture.loadFromGame("data/character/sanae/gageBb000.cv0");
	gagesEffects[1].rect.width = gagesEffects[1].texture.getSize().x;
	gagesEffects[1].rect.height = gagesEffects[1].texture.getSize().y;

	gagesEffects[2].texture.loadFromGame("data/character/sanae/gageCa000.cv0");
	gagesEffects[2].setSize(gagesEffects[2].texture.getSize());
	gagesEffects[2].rect.width = gagesEffects[2].texture.getSize().x;
	gagesEffects[2].rect.height = gagesEffects[2].texture.getSize().y;
}

int __fastcall CProfileDeckEdit_OnProcess(SokuLib::ProfileDeckEdit *This)
{
	auto keys = reinterpret_cast<SokuLib::KeyManager *>(0x89A394);
	bool ogDialogsActive = This->guideVector[3].active || This->guideVector[2].active;

	profileSelectReady = false;
	selectDeckGuide.active = This->cursorOnDeckChangeBox && editSelectedDeck != editedDecks.size() - 1 && !ogDialogsActive;
	createDeckGuide.active = This->cursorOnDeckChangeBox && editSelectedDeck == editedDecks.size() - 1 && !ogDialogsActive;
	This->guideVector[4].active = false;
	if (renameBoxDisplayed && ogDialogsActive) {
		renameBoxDisplayed = false;
		((int(__thiscall*) (void*, bool))0x40ea10)((void*)0x8A02F0, true);
	}
	deleteBoxDisplayed &= !ogDialogsActive;
	copyBoxDisplayed   &= !ogDialogsActive;

	bool renameBox = renameBoxDisplayed;
	bool deleteBox = deleteBoxDisplayed;
	bool copyBox   = copyBoxDisplayed;

	editBoxGuide.active = renameBoxDisplayed;
	if (ogDialogsActive);
	else if (renameBox)
		renameBoxUpdate(keys);
	else if (deleteBox)
		deleteBoxUpdate(keys);
	else if (copyBox)
		copyBoxUpdate(keys);
	else if (keys->keymapManager->input.a == 1 && This->cursorOnDeckChangeBox) {
		if (editSelectedDeck == editedDecks.size() - 1)
			openCopyBox(This);
		else
			openRenameBox(This);
	} else if (keys->keymapManager->input.c && This->cursorOnDeckChangeBox && editSelectedDeck != editedDecks.size() - 1)
		openDeleteBox();
	else if (keys->keymapManager->input.c && This->cursorOnDeckChangeBox) {
		editedDecks.back().name = "Deck #" + std::to_string(editedDecks.size());
		editedDecks.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
		openRenameBox(This);
	}
	if (renameBox || deleteBox || copyBox) {
		selectDeckGuide.active = false;
		createDeckGuide.active = false;
		This->guideVector[0].active = false;
		escPressed = ((int *)0x8998D8)[1];
		((int *)0x8998D8)[1] = 0;
		((int *)0x8998D8)[DIK_F1] = 0;
		memset(&keys->keymapManager->input, 0, sizeof(keys->keymapManager->input));
	} else if (escPressed) {
		escPressed = ((int *)0x8998D8)[1];
		((int *)0x8998D8)[1] = 0;
	} else if (editSelectedDeck == editedDecks.size() - 1 && This->displayedNumberOfCards != 0) {
		SokuLib::playSEWaveBuffer(0x28);
		editedDecks.back().name = "Deck #" + std::to_string(editedDecks.size());
		editedDecks.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	}
	// This hides the deck select arrow
	((bool ***)This)[0x10][0x2][20] = false;
	updateGuide(selectDeckGuide);
	updateGuide(createDeckGuide);
	updateGuide(editBoxGuide);
	return (This->*s_originalCProfileDeckEdit_OnProcess)();
}

int __fastcall CProfileDeckEdit_OnRender(SokuLib::ProfileDeckEdit *This)
{
	int ret = (This->*s_originalCProfileDeckEdit_OnRender)();

	SokuLib::DrawUtils::Sprite &sprite = cardsTextures[SokuLib::CHARACTER_RANDOM][21];
	SokuLib::DrawUtils::Sprite textSprite;
	SokuLib::Vector2i pos{38, 88};
	int text;
	int width = 0;

	if (saveError) {
		saveError = false;
		This->editedDeck->begin()->second--;
	}

	sprite.rect.top = sprite.rect.width = 0;
	sprite.rect.width = sprite.texture.getSize().x;
	sprite.rect.height = sprite.texture.getSize().y;
	sprite.tint = SokuLib::Color::White;
	sprite.setSize({20, 32});
	for (int i = 20; i > This->displayedNumberOfCards; i--) {
		sprite.setPosition({304 + 24 * ((i - 1) % 10), 260 + 38 * ((i - 1) / 10)});
		sprite.draw();
	}

	if (errorCounter) {
		float alpha = min(1.f, errorCounter / 30.f);

		errorCounter--;
		if (!SokuLib::textureMgr.createTextTexture(&text, "Please keep the number of cards in the deck at or below 20", font, TEXTURE_SIZE, FONT_HEIGHT + 18, &width, nullptr)) {
			puts("C'est vraiment pas de chance");
			return ret;
		}

		auto realX = 53;

		if (errorCounter >= 105) {
			realX += sokuRand(31) - 15;
			if (errorCounter >= 115)
				alpha = 1 - (errorCounter - 115.f) / 5;
		}

		textSprite.setPosition({realX, pos.y - 20});
		textSprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
		textSprite.setSize({TEXTURE_SIZE, FONT_HEIGHT + 18});
		textSprite.rect = {
			0, 0, TEXTURE_SIZE, FONT_HEIGHT + 18
		};
		textSprite.tint = SokuLib::Color::Red * alpha;
		textSprite.draw();
	}

	if (!SokuLib::textureMgr.createTextTexture(&text, editedDecks[editSelectedDeck].name.c_str(), font, TEXTURE_SIZE, FONT_HEIGHT + 18, &width, nullptr)) {
		puts("C'est vraiment pas de chance");
		return ret;
	}
	pos.x = 153 - width / 2;
	textSprite.setPosition(pos);
	textSprite.texture.setHandle(text, {TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.setSize({TEXTURE_SIZE, FONT_HEIGHT + 18});
	textSprite.rect = {
		0, 0, TEXTURE_SIZE, FONT_HEIGHT + 18
	};
	textSprite.tint = SokuLib::Color::White;
	textSprite.draw();

	pos.x -= 32;
	pos.y -= 6;
	arrowSprite.rect = {0, 0, 32, 32};
	arrowSprite.setPosition(pos);
	arrowSprite.setSize({33, 33});
	arrowSprite.tint = SokuLib::Color::White;
	arrowSprite.draw();

	pos.x += 32 + width;
	arrowSprite.rect.left = 32;
	arrowSprite.setPosition(pos);
	arrowSprite.draw();

	renderGuide(selectDeckGuide);
	renderGuide(createDeckGuide);
	renderGuide(editBoxGuide);
	if (renameBoxDisplayed)
		renameBoxRender();
	else if (deleteBoxDisplayed)
		deleteBoxRender();
	else if (copyBoxDisplayed)
		copyBoxRender();
	return ret;
}

SokuLib::ProfileDeckEdit *__fastcall CProfileDeckEdit_Destructor(SokuLib::ProfileDeckEdit *This, int, unsigned char param)
{
	auto setup_global = (int(__thiscall*) (void*, bool))0x40ea10;

	setup_global((void*)0x8A02F0, false);
	return (This->*s_originalCProfileDeckEdit_Destructor)(param);
}

SokuLib::ProfileDeckEdit *__fastcall CProfileDeckEdit_Init(SokuLib::ProfileDeckEdit *This, int, int param_2, int param_3, SokuLib::Sprite *param_4)
{
	auto ret = (This->*og_CProfileDeckEdit_Init)(param_2, param_3, param_4);

	if (profileSelectReady)
		return ret;
	profileSelectReady = true;
	errorCounter = 0;
	editSelectedDeck = 0;
	if (editSelectedProfile != 4) {
		loadedDecks[4] = loadedDecks[editSelectedProfile];
		for (auto &val : loadedDecks[4])
			val.second.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	} else if (loadedDecks[4][This->editedCharacter].empty()) {
		loadedDecks[4] = loadedDecks[0];
		for (auto &val : loadedDecks[4])
			val.second.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	}
	editedDecks = loadedDecks[4][This->editedCharacter];
	loadDeckToGame(This, editedDecks[editSelectedDeck].cards);
	deleteBoxDisplayed = false;
	renameBoxDisplayed = false;
	copyBoxDisplayed = false;
	return ret;
}

static void sanitizeDeck(SokuLib::Character chr, Deck &deck)
{
	unsigned last = 100 + nbSkills[chr];
	std::map<unsigned short, unsigned char> used;
	std::vector<unsigned short> cards;
	auto name = names.find(chr);

	if (name == names.end())
		return;
	for (int i = 0; i <= 21; i++)
		cards.push_back(i);
	for (int i = 100; i < last; i++)
		cards.push_back(i);
	for (auto &card : characterSpellCards[chr])
		cards.push_back(card);
	for (int i = 0; i < 20; i++) {
		auto &card = deck.cards[i];

		if (card == 21)
			continue;
		if (std::find(cards.begin(), cards.end(), card) == cards.end() || used[card] >= 4) {
			card = 21;
			continue;
		}
		used[card]++;
	}
}

static bool loadOldProfileFile(nlohmann::json &json, std::map<unsigned char, std::vector<Deck>> &map, int index)
{
	if (json.size() != 20)
		throw std::invalid_argument("Not 20 characters");
	for (auto &arr : json) {
		for (auto &elem : arr) {
			elem.contains("name") && (elem["name"].get<std::string>(), true);
			if (!elem.contains("cards") || elem["cards"].get<std::vector<unsigned short>>().size() != 20)
				throw std::invalid_argument(elem.dump());
		}
	}
	for (auto &elem : map)
		elem.second.clear();
	for (int i = 0; i < 20; i++) {
		auto &array = json[i];

		for (int j = 0; j < array.size(); j++) {
			auto &elem = array[j];
			Deck deck;

			if (!elem.contains("name"))
				deck.name = "Deck #" + std::to_string(j + 1);
			else
				deck.name = elem["name"];
			memcpy(deck.cards.data(), elem["cards"].get<std::vector<unsigned short>>().data(), sizeof(*deck.cards.data()) * deck.cards.size());
			sanitizeDeck(static_cast<SokuLib::Character>(i), deck);
			std::sort(deck.cards.begin(), deck.cards.end());
			map[i].push_back(deck);
		}
		if (index == 4)
			map[i].push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	}
	return true;
}

bool allDecksDefault(unsigned short (*decks)[4][20], unsigned i)
{
	for (int j = 0; j < 4; j++) {
		std::sort(decks[i][j], decks[i][j] + 20);
		if (i >= (hasSoku2 ? defaultDecks.size() + 2 : 20) || memcmp(defaultDecks[i].data(), decks[i][j], sizeof(defaultDecks[i])) != 0)
			return false;
	}
	return true;
}

static bool convertProfile(const char *jsonPath)
{
	char path[MAX_PATH];
	unsigned char length;
	unsigned short cards[255];
	FILE *json;
	FILE *profile;
	unsigned short (*decks)[4][20] = nullptr;
	int size;

	strcpy(path, jsonPath);
	*strrchr(path, '.') = 0;
	strcat(path, ".pf");
	printf("Loading decks from profile file %s to %s.\n", path, jsonPath);

	profile = fopen(path, "r");
	if (!profile) {
		printf("Can't open %s for reading %s\n", path, strerror(errno));
		return false;
	}

	json = fopen(jsonPath, "w");
	if (!json) {
		fclose(profile);
		printf("Can't open %s for writing %s\n", jsonPath, strerror(errno));
		return false;
	}

	fseek(profile, 106, SEEK_SET);
	for (size = 1; !feof(profile); size++) {
		decks = static_cast<unsigned short (*)[4][20]>(realloc(decks, sizeof(*decks) * size));
		for (int k = 0; k < 4; k++) {
			fread(&length, sizeof(length), 1, profile);
			fread(cards, sizeof(*cards), length, profile);
			for (int j = length; j < 20; j++)
				cards[j] = 21;
			memcpy(decks[size - 1][k], cards, 40);
		}
	}
	fclose(profile);

	fwrite("{", 1, 1, json);

	const char *deckNames[4] = {
		"yorokobi",
		"ikari",
		"ai",
		"tanoshii"
	};
	bool first = true;
	bool first2 = true;
	unsigned i = 0;

	size -= 2;
	if (size > 20)
		size -= 2;
	printf("There are %i characters...\n", size);
	while (size--) {
		if (allDecksDefault(decks, i)) {
			printf("Character %i has all default decks\n", i);
			i++;
			if (i == 20)
				i += 2;
			continue;
		}
		fprintf(json, "%s\n\t\"%i\": [", first2 ? "" : ",", i);
		first2 = false;
		first = true;
		for (int j = 0; j < 4; j++) {
			std::sort(decks[i][j], decks[i][j] + 20);
			if (i < (hasSoku2 ? defaultDecks.size() + 2 : 20) && memcmp(defaultDecks[i].data(), decks[i][j], sizeof(defaultDecks[i])) == 0)
				continue;
			fprintf(json, "%s\n\t\t{\n\t\t\t\"name\": \"%s\",\n\t\t\t\"cards\": [", first ? "" : ",", deckNames[j]);
			first = false;
			for (int k = 0; k < 20; k++)
				fprintf(json, "%s%i", k == 0 ? "" : ", ", decks[i][j][k]);
			fwrite("]\n\t\t}", 1, 5, json);
		}
		fwrite("\n\t]", 1, 3, json);
		i++;
		if (i == 20)
			i += 2;
	}
	fwrite("\n}", 1, 2, json);
	fclose(json);
	free(decks);
	decks = nullptr;
	return true;
}

static bool saveProfile(const std::string &path, const std::map<unsigned char, std::vector<Deck>> &profile)
{
	nlohmann::json result;

	printf("Saving to %s\n", path.c_str());
	for (auto &elem : profile) {
		if (elem.second.empty())
			continue;

		auto &array = result[std::to_string(elem.first)];

		array = nlohmann::json::array();
		for (auto &deck : elem.second) {
			array.push_back({
				{"name", deck.name},
				{"cards", std::vector<unsigned short>{
					deck.cards.begin(),
					deck.cards.end()
				}}
			});
		}
	}
	if (std::ifstream(path + ".bck").fail())
		rename(path.c_str(), (path + ".bck").c_str());

	auto resultStr = result.dump(4);
	std::ofstream stream{path};

	if (stream.fail()) {
		MessageBoxA(SokuLib::window, ("Cannot open \"" + path + "\". Please make sure you have proper permissions and enough space on disk.").c_str(), "Saving error", MB_ICONERROR);
		return false;
	}
	stream << resultStr;
	if (stream.fail()) {
		stream.close();
		MessageBoxA(SokuLib::window, ("Cannot write to \"" + path + "\". Please make sure you have proper enough space on disk.").c_str(), "Saving error", MB_ICONERROR);
		return false;
	}
	stream.close();
	unlink((path + ".bck").c_str());
	return true;
}

static bool loadProfileFile(const std::string &path, std::ifstream &stream, std::map<unsigned char, std::vector<Deck>> &map, int index, bool hasBackup = false)
{
	if (stream.fail()) {
		printf("Failed to open file %s: %s\n", path.c_str(), strerror(errno));
		if (hasBackup)
			throw std::exception();
		if (errno == ENOENT) {
			puts("Let's fix that");
			if (convertProfile(path.c_str())) {
				stream.open(path);
				if (stream)
					return loadProfileFile(path, stream, map, index);
			}
		}
		for (auto &elem : names)
			map[elem.first].clear();
		if (index == 4)
			for (auto &elem : map)
				elem.second.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
		return false;
	}

	nlohmann::json json;

	stream >> json;
	if (json.is_array()) {
		printf("%s is in the old format. Converting...\n", path.c_str());
		loadOldProfileFile(json, map, index);
		stream.close();
		saveProfile(path, map);
		return true;
	}
	if (json.is_null()) {
		for (auto &elem : names)
			map[elem.first].clear();
		if (index == 4)
			for (auto &elem : map)
				elem.second.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
		return true;
	}
	if (!json.is_object())
		throw std::invalid_argument("JSON is neither an array nor an object");
	for (auto &arr : json.items()) {
		std::stoi(arr.key());
		for (auto &elem : arr.value()) {
			elem.contains("name") && (elem["name"].get<std::string>(), true);
			if (!elem.contains("cards") || elem["cards"].get<std::vector<unsigned short>>().size() != 20)
				throw std::invalid_argument(elem.dump());
		}
	}
	for (auto &elem : names)
		map[elem.first].clear();
	for (auto &arr : json.items()) {
		auto &array = arr.value();
		auto index = std::stoi(arr.key());

		for (int j = 0; j < array.size(); j++) {
			auto &elem = array[j];
			Deck deck;

			if (!elem.contains("name"))
				deck.name = "Deck #" + std::to_string(j + 1);
			else
				deck.name = elem["name"];
			memcpy(deck.cards.data(), elem["cards"].get<std::vector<unsigned short>>().data(), sizeof(*deck.cards.data()) * deck.cards.size());
			sanitizeDeck(static_cast<SokuLib::Character>(index), deck);
			std::sort(deck.cards.begin(), deck.cards.end());
			map[index].push_back(deck);
		}
	}

	std::map<unsigned, std::vector<unsigned>> duplicateDecks;
	size_t duplicateCount = 0;

	for (auto &[key, array] : map) {
		std::vector<std::string> foundNames;
		std::vector<unsigned> duplicates;

		for (size_t i = 0; i < array.size(); i++) {
			if (array[i].name == "Create new deck" || std::find(foundNames.begin(), foundNames.end(), array[i].name) != foundNames.end())
				duplicates.push_back(i);
			else
				foundNames.push_back(array[i].name);
		}
		if (!duplicates.empty())
			duplicateDecks[key] = duplicates;
		duplicateCount += duplicates.size();
	}
	if (!duplicateDecks.empty() && MessageBoxA(
		nullptr,
		(std::to_string(duplicateCount) + " duplicate decks found in " + path + ". Do you want to delete them?").c_str(),
		"InfiniteDecks duplication error",
		MB_ICONQUESTION | MB_YESNO
	) == IDYES) {
		for (auto &[key, array] : duplicateDecks) {
			std::sort(array.begin(), array.end(), [](unsigned a, unsigned b){
				return a > b;
			});
			for (unsigned i : array)
				map[key].erase(map[key].begin() + i);
		}
		saveProfile(path, map);
	}
	if (index == 4)
		for (auto &elem : map)
			elem.second.push_back({"Create new deck", {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21}});
	return true;
}

static void __fastcall handleProfileChange(SokuLib::Profile *This, SokuLib::String *val)
{
	initAssets();

	const char *arg = *val;
	std::string profileName{arg, strstr(arg, ".pf")};
	std::string profile = "profile/" + profileName + ".json";
	int index = 4;
	bool hasBackup;

	if (This == &SokuLib::profile1)
		index = 0;
	if (This == &SokuLib::profile2)
		index = 1;
	if (index != 4)
		loadedProfiles_[index] = profileName;
	printf("Loading %s in buffer %i\n", profile.c_str(), index);

	bool result = false;
	auto &arr = loadedDecks[index];
	std::ifstream stream{profile + ".bck"};

	hasBackup = !stream.fail();
	stream.close();
	if (hasBackup)
		printf("%s has backup data !\n", profile.c_str());

	stream.open(profile, std::ifstream::in);
	try {
		result = loadProfileFile(profile, stream, arr, index, hasBackup);
	} catch (std::exception &e) {
		auto answer = IDNO;

		if (!hasBackup)
			MessageBoxA(SokuLib::window, ("Cannot load file " + profile + ": " + e.what()).c_str(), "Fatal error", MB_ICONERROR);
		else
			answer = MessageBoxA(SokuLib::window, ("Cannot load file " + profile + ": " + e.what() + "\n\nDo you want to load backup file ?").c_str(), "Loading error", MB_ICONERROR | MB_YESNO);
		if (answer != IDYES) {
			try {
				result = loadProfileFile(profile, stream, arr, index);
			} catch (std::exception &e) {
				MessageBoxA(SokuLib::window, ("Cannot load file " + profile + ": " + e.what()).c_str(), "Fatal error", MB_ICONERROR);
				throw;
			}
		}
	}
	stream.close();

	if (!result && hasBackup) {
		try {
			stream.open(profile + ".bck", std::ifstream::in);
			printf("Loading %s\n", (profile + ".bck").c_str());
			result = loadProfileFile(profile + ".bck", stream, arr, index);
			stream.close();
		} catch (std::exception &e) {
			MessageBoxA(SokuLib::window, ("Cannot load file " + profile + ".bck: " + e.what()).c_str(), "Fatal error", MB_ICONERROR);
			throw;
		}
		unlink(profile.c_str());
		rename((profile + ".bck").c_str(), profile.c_str());
		lastLoadedProfile = profileName;
		return;
	}

	lastLoadedProfile = profileName;
	if (hasBackup)
		unlink((profile + ".bck").c_str());
}

void __declspec(naked) onProfileChanged()
{
	__asm {
		MOV ECX, ESI;
		MOV EDX, [ESP + 0x18];
		JMP handleProfileChange
	}
}

static void onDeckSaved()
{
	auto menu = SokuLib::getMenuObj<SokuLib::ProfileDeckEdit>();
	std::string path;

	if (!saveDeckFromGame(menu, editedDecks[editSelectedDeck].cards))
		return;

	loadedDecks[4][menu->editedCharacter] = editedDecks;

	auto toSave = loadedDecks[4];

	for (auto &elem : toSave)
		elem.second.pop_back();
	if (editSelectedProfile != 4) {
		loadedDecks[editSelectedProfile] = toSave;
		path = "profile/" + loadedProfiles_[editSelectedProfile] + ".json";
	} else
		path = "profile/" + lastLoadedProfile + ".json";

	if (!saveProfile(path, toSave)) {
		if (menu->displayedNumberOfCards == 20) {
			menu->editedDeck->begin()->second++;
			saveError = true;
		}
		return;
	}

	for (auto &card : *menu->editedDeck)
		card.second = 0;
	for (int i = 0; i < 20; i++)
		(*menu->editedDeck)[i] = 1;
}

void drawGradiantBar(float x, float y, float maxY)
{
	if (y == 114)
		y = 90;
	s_originalDrawGradiantBar(x, y, maxY);
}

void __declspec(naked) selectProfileSpriteColor()
{
	__asm {
		MOVSX EAX, byte ptr [EDI + 0x70]
		MOV ECX, EAX
		AND ECX, 1
		RET
	}
}

unsigned chrKeysContinue = 0x46C908;
unsigned chrKeysSkip = 0x46CA0C;

bool initStartingKeys(SokuLib::v2::Player * const assist)
{
	auto &chr = (&currentChr.first)[currentIndex(assist) & 1];

	if (currentIndex(assist) < 2) {
		if (chr.tagging) {
			if (chr.deathTag && chr.tagTimer < 60)
				return false;
			if (chr.slowTag && chr.tagTimer < SLOW_TAG_STARTUP)
				return false;
		}
		return true;
	}
	assist->inputData.keyInput.changeCard = 0;
	if (assist->renderInfos.yRotation == 90) {
		if (!loadedData[chr.chr][chr.loadoutIndex].canFly) {
			assist->inputData.keyInput.horizontalAxis = 0;
			assist->inputData.keyInput.verticalAxis = 0;
			assist->inputData.keyInput.a = 0;
			assist->inputData.keyInput.b = assist->keyManager->keymapManager->input.select && assist->inputData.keyInput.b ? assist->inputData.keyInput.b + 1 : 0;
			assist->inputData.keyInput.c = assist->keyManager->keymapManager->input.select && assist->inputData.keyInput.c ? assist->inputData.keyInput.c + 1 : 0;
			assist->inputData.keyInput.d = 0;
			assist->inputData.keyInput.spellcard = 0;
			return false;
		}

		if (assist->keyManager->keymapManager->input.horizontalAxis == 0)
			assist->inputData.keyInput.horizontalAxis = 0;
		else if (std::copysign(1, assist->keyManager->keymapManager->input.horizontalAxis) == std::copysign(1, assist->inputData.keyInput.horizontalAxis))
			assist->inputData.keyInput.horizontalAxis += std::copysign(1, assist->keyManager->keymapManager->input.horizontalAxis);
		else
			assist->inputData.keyInput.horizontalAxis = std::copysign(1, assist->keyManager->keymapManager->input.horizontalAxis);

		if (assist->keyManager->keymapManager->input.verticalAxis == 0)
			assist->inputData.keyInput.verticalAxis = 0;
		else if (std::copysign(1, assist->keyManager->keymapManager->input.verticalAxis) == std::copysign(1, assist->inputData.keyInput.verticalAxis))
			assist->inputData.keyInput.verticalAxis += std::copysign(1, assist->keyManager->keymapManager->input.verticalAxis);
		else
			assist->inputData.keyInput.verticalAxis = std::copysign(1, assist->keyManager->keymapManager->input.verticalAxis);

		if (assist->keyManager->keymapManager->input.d)
			assist->inputData.keyInput.d++;
		else
			assist->inputData.keyInput.d = 0;

		if (assist->inputData.keyInput.d == 0) {
			assist->inputData.keyInput.verticalAxis = 0;
			assist->inputData.keyInput.horizontalAxis = 0;
		}
		assist->inputData.keyInput.a = 0;
		assist->inputData.keyInput.b = 0;
		assist->inputData.keyInput.c = 0;
		assist->inputData.keyInput.spellcard = 0;
	} else if (chr.starting) {
		for (int i = 0; i < 8; i++) {
			int &old = ((int *) &assist->inputData.keyInput)[i];
			int expected = ((int *) &chr.startKeys)[i];

			if (i == 6)
				continue;
			if (expected < 0)
				old--;
			else if (expected > 0)
				old++;
			else if ((i != 3 && i != 4) || !old || ((int *)&assist->keyManager->keymapManager->input)[i] == 0)
				old = 0;
			else
				old++;
		}
	} else if (chr.started && chr.ended && chr.nb == 0) {
		switch (chr.currentStance) {
		default:
			assist->inputData.keyInput.horizontalAxis = 0;
			assist->inputData.keyInput.verticalAxis = 0;
			assist->inputData.keyInput.a = 0;
			assist->inputData.keyInput.b = assist->keyManager->keymapManager->input.select && assist->inputData.keyInput.b ? assist->inputData.keyInput.b + 1 : 0;
			assist->inputData.keyInput.c = assist->keyManager->keymapManager->input.select && assist->inputData.keyInput.c ? assist->inputData.keyInput.c + 1 : 0;
			assist->inputData.keyInput.d = 0;
			assist->inputData.keyInput.spellcard = 0;
			return false;
		case 1:
			if (assist->inputData.keyInput.verticalAxis < 0)
				assist->inputData.keyInput.verticalAxis = 0;
			assist->inputData.keyInput.verticalAxis++;
			assist->inputData.keyInput.horizontalAxis = 0;
			assist->inputData.keyInput.a = 0;
			assist->inputData.keyInput.b = 0;
			assist->inputData.keyInput.b = 0;
			assist->inputData.keyInput.c = 0;
			assist->inputData.keyInput.spellcard = 0;
			break;
		case 3:
			if (assist->inputData.keyInput.verticalAxis > 0)
				assist->inputData.keyInput.verticalAxis = 0;
			if ((chr.stanceCtr & 0x1F) < 0x10) {
				assist->inputData.keyInput.d++;
				assist->inputData.keyInput.verticalAxis--;
			} else {
				assist->inputData.keyInput.d = 0;
				assist->inputData.keyInput.verticalAxis = 0;
			}
			assist->inputData.keyInput.horizontalAxis = 0;
			assist->inputData.keyInput.a = 0;
			assist->inputData.keyInput.b = 0;
			assist->inputData.keyInput.c = 0;
			assist->inputData.keyInput.spellcard = 0;
			break;
		}
	} else if (chr.started) {
		if (chr.canControl)
			return true;

		for (int i = 0; i < 8; i++) {
			int &old = ((int *)&assist->inputData.keyInput)[i];
			int actual = assist->keyManager->keymapManager->input.select;
			int actual2 = ((int *)&assist->keyManager->keymapManager->input)[i];
			int authorized = ((int *)&chr.allowedKeys)[i];
			int start = ((int *)&chr.startKeys)[i];

			if (i == 6)
				continue;
			if (i >= 2) {
				if (actual && authorized || start)
					old++;
				else if ((i != 3 && i != 4) || !old || ((int *)&assist->keyManager->keymapManager->input)[i] == 0)
					old = 0;
				else
					old++;
			} else {
				if (actual * old < 0)
					old = 0;
				if (authorized == 2) {
					if (actual2 == 0)
						old = 0;
					else if (actual2 < 0)
						old--;
					else
						old++;
				} else {
					if (!authorized || actual == 0)
						old = 0;
					else if (actual < 0)
						old--;
					else
						old++;
				}
			}
		}
	} else if (chr.startup >= 1 || assist->renderInfos.yRotation > 10) {
		assist->inputData.keyInput.horizontalAxis = 0;
		assist->inputData.keyInput.verticalAxis = 0;
		assist->inputData.keyInput.a = 0;
		assist->inputData.keyInput.b = 0;
		assist->inputData.keyInput.c = 0;
		assist->inputData.keyInput.d = 0;
		assist->inputData.keyInput.spellcard = 0;
	}
	return false;
}

void renderKeysResult(const bool b, SokuLib::v2::Player * const assist)
{
	if (currentIndex(assist) != 2)
		return;
	printf(
		"%s (%i): h %i v %i a %i b %i c %i d %i se %i pa %i ch %i sp %i | h %i v %i a %i b %i c %i d %i ch %i sp %i\n",
		b ? "true" : "false", b,
		assist->keyManager->keymapManager->input.horizontalAxis,
		assist->keyManager->keymapManager->input.verticalAxis,
		assist->keyManager->keymapManager->input.a,
		assist->keyManager->keymapManager->input.b,
		assist->keyManager->keymapManager->input.c,
		assist->keyManager->keymapManager->input.d,
		assist->keyManager->keymapManager->input.select,
		assist->keyManager->keymapManager->input.pause,
		assist->keyManager->keymapManager->input.changeCard,
		assist->keyManager->keymapManager->input.spellcard,
		assist->inputData.keyInput.horizontalAxis,
		assist->inputData.keyInput.verticalAxis,
		assist->inputData.keyInput.a,
		assist->inputData.keyInput.b,
		assist->inputData.keyInput.c,
		assist->inputData.keyInput.d,
		assist->inputData.keyInput.changeCard,
		assist->inputData.keyInput.spellcard
	);
}

void __declspec(naked) onChrKeys()
{
	__asm {
		PUSH EDI
		PUSH ESI
		CALL initStartingKeys
		PUSH EAX
		CALL renderKeysResult
		POP EAX
		POP ESI
		POP EDI
		TEST AL, AL
		JNZ keep
		MOV EBX, dword ptr [ESI + 0x75c]
		MOV EAX, dword ptr [ESI + 0x760]
		MOV ECX, dword ptr [ESI + 0x764]
		MOV EDX, dword ptr [ESI + 0x768]
		MOV EBP, dword ptr [ESI + 0x76c]
		MOV dword ptr [ESP + 0x1c], EBP
		MOV EBP, dword ptr [ESI + 0x770]
		MOV dword ptr [ESP + 0x20], EBP
		JMP [chrKeysSkip]
	keep:
		JMP [chrKeysContinue]
	}
}

const auto weatherFunction = (void (*)())0x4388e0;

__declspec(naked) void weatherFct()
{
	__asm {
		PUSH 0x14
		ADD ECX, 0x130
		CALL weatherFunction
		RET
	}
}

const unsigned selectRandomDeck_hook_retAddr = 0x4208CC;

__declspec(naked) void selectRandomDeck_hook()
{
	__asm {
		MOV byte ptr [ESI + EBP * 1 + 0x22C0], 3
		JMP [selectRandomDeck_hook_retAddr]
	}
}

const unsigned chrSelectLastStep_hook_retAddr = 0x4208CC;

__declspec(naked) void chrSelectLastStep_hook()
{
	__asm {
		// Ptr to the state
		LEA ECX, [ESI + EBP * 1 + 0x22C0]
		PUSH ECX
		// Index
		PUSH EBP
		// Input handler
		MOV EDX, EAX
		// Animation object for the character
		MOV ECX, [EBX + 0x22B8]
		CALL chrSelectLastStep
		JMP [chrSelectLastStep_hook_retAddr]
	}
}

constexpr unsigned step3Location = 0x420502;
constexpr unsigned breakLocation = 0x420775;

void __declspec(naked) chrSelectExtraSteps_hook()
{
	__asm {
		JZ case3
		PUSH EBP
		PUSH EDI
		PUSH EBX
		PUSH ESI
		MOV ECX, ESI
		MOV EDX, EBP
		CALL updateCharacterSelect2
		POP ESI
		POP EBX
		POP EDI
		POP EBP
		JMP [breakLocation]
	case3:
		JMP [step3Location]
	}
}

unsigned grInputHook = 0;
constexpr unsigned controllerUpdateOverrideRet = 0x46C902;

void __declspec(naked) controllerUpdateOverride()
{
	__asm {
		CMP [ESI + 0x14E], 2
		PUSH EBX
		PUSH EBP
		PUSH EDI
		MOV EDI, [EAX]
		MOV EAX, [EDI]
		MOV EDX, [EAX + 4]
		MOV ECX, EDI
		JGE noUpdateController
		PUSH EDX
		MOV EDX, [grInputHook]
		TEST EDX, EDX
		JNZ toGR
		POP EDX
		CALL EDX
	noUpdateController:
		JMP [controllerUpdateOverrideRet]
	toGR:
		POP EDX
		JMP [grInputHook]
	}
}

static_assert(sizeof(*chrSelectExtra) == 744);
static_assert(offsetof(ExtraChrSelectData, cursorCounter) == 728);

void __declspec(naked) firstChrSelected()
{
	__asm {
		LEA EAX, [chrSelectExtra]
		TEST EBP, EBP
		JZ zero
		MOV [EAX + 728 + 744], 60
		MOV byte ptr [ESI + EBP * 0x1 + 0x22C0], 0x4
		RET
	zero:
		MOV [EAX + 728], 60
		MOV byte ptr [ESI + EBP * 0x1 + 0x22C0], 0x4
		RET
	}
}

void __declspec(naked) disableP34Pause()
{
	__asm {
		MOVSX EDX, byte ptr [ECX + 0x14E]
		CMP EDX, 1
		JG end
		MOV EDX, dword ptr [EAX]
		CMP dword ptr [EDX + 0x58], 0x1
	end:
		RET
	}
}

struct GiurollCallbacks {
	unsigned (*saveState)();
	void (*loadStatePre)(size_t frame, unsigned);
	void (*loadStatePost)(unsigned);
	void (*freeState)(unsigned);
};

class SavedFrame {
private:
	std::pair<ChrInfo, ChrInfo> _currentChr;
	SokuLib::v2::Player *_players1[4];
	SokuLib::v2::Player *_players2[4];
	SokuLib::v2::Player *_players3[4];
	SokuLib::v2::Player *_playersHud1[2];
	SokuLib::v2::Player *_playersHud2[2];
	std::pair<int, int> _hud1HP[2];
	std::pair<int, int> _hud2HP[2];
	short *_spiritPointer[10];
	short *_timeWithBrokenOrbPointer[10];
	short *_cardGaugePointer[10];

public:
	SavedFrame() :
		_currentChr(currentChr)
	{
		auto players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0xC);

		this->_currentChr.first.meter = currentChr.first.meter;
		this->_currentChr.second.meterReq = currentChr.second.meterReq;
		this->_currentChr.second.burstCharges = currentChr.second.burstCharges;
		this->_currentChr.second.meter = currentChr.second.meter;
		this->_currentChr.second.meterReq = currentChr.second.meterReq;
		this->_currentChr.second.burstCharges = currentChr.second.burstCharges;
		for (int i = 0; i < 4; i++) {
			this->_players1[i] = players[i];
			this->_players2[i] = SokuLib::v2::GameDataManager::instance->players[i];
			this->_players3[i] = SokuLib::v2::GameDataManager::instance->activePlayers[i];
		}
		for (int i = 0; i < 2; i++) {
			this->_playersHud1[i] = hud1->playerHUD[i].player;
			this->_playersHud2[i] = hud2.playerHUD[i].player;
			this->_hud1HP[i].first = hud1->playerHUD[i].lifebarYellowValue;
			this->_hud1HP[i].second = hud1->playerHUD[i].lifebarRedValue;
			this->_hud2HP[i].first = hud2.playerHUD[i].lifebarYellowValue;
			this->_hud2HP[i].second = hud2.playerHUD[i].lifebarRedValue;
			for (int j = 0; j < 5; j++) {
				this->_spiritPointer[i * 5 + j] = ((short **)(hud1->playerHUD[i].orbsGauge[j]->gauge.value))[1];
				this->_timeWithBrokenOrbPointer[i * 5 + j] = ((short **)(hud1->playerHUD[i].orbsCrushGauge[j]->gauge.value))[1];
				this->_cardGaugePointer[i * 5 + j] = ((short **)(hud1->playerHUD[i].cardGauge[j]->gauge.value))[1];
			}
		}
	}

	void restorePre()
	{
		auto players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0xC);

		for (int i = 0; i < 4; i++) {
			players[i] = this->_players1[i];
			SokuLib::v2::GameDataManager::instance->players[i] = this->_players2[i];
			SokuLib::v2::GameDataManager::instance->activePlayers[i] = this->_players3[i];
		}
		for (int i = 0; i < 2; i++) {
			hud1->playerHUD[i].lifebarYellowValue = this->_hud1HP[i].first;
			hud1->playerHUD[i].lifebarRedValue = this->_hud1HP[i].second;
			hud2.playerHUD[i].lifebarYellowValue = this->_hud2HP[i].first;
			hud2.playerHUD[i].lifebarRedValue = this->_hud2HP[i].second;
			hud1->playerHUD[i].player = this->_playersHud1[i];
			hud2.playerHUD[i].player = this->_playersHud2[i];
			for (int j = 0; j < 5; j++) {
				((short **)(hud1->playerHUD[i].orbsGauge[j]->gauge.value))[1] = this->_spiritPointer[i * 5 + j];
				((short **)(hud1->playerHUD[i].orbsCrushGauge[j]->gauge.value))[1] = this->_timeWithBrokenOrbPointer[i * 5 + j];
				((short **)(hud1->playerHUD[i].cardGauge[j]->gauge.value))[1] = this->_cardGaugePointer[i * 5 + j];
			}
		}
		players[0]->gameData.opponent = players[1];
		players[1]->gameData.opponent = players[0];
		players[0]->gameData.ally = players[0];
		players[1]->gameData.ally = players[1];
		players[2]->gameData.opponent = players[1];
		players[3]->gameData.opponent = players[0];
		players[3]->gameData.ally = players[0];
		players[2]->gameData.ally = players[1];
		for (int i = 0; i < 4; i++)
			currentIndex(players[i]) = i;
	}

	void restorePost()
	{
		currentChr = this->_currentChr;
		currentChr.first.meter = this->_currentChr.first.meter;
		currentChr.first.meterReq = this->_currentChr.first.meterReq;
		currentChr.first.burstCharges = this->_currentChr.first.burstCharges;
		currentChr.second.meter = this->_currentChr.second.meter;
		currentChr.second.meterReq = this->_currentChr.second.meterReq;
		currentChr.second.burstCharges = this->_currentChr.second.burstCharges;
	}
};

unsigned lastFrameId = 0;
std::map<unsigned, SavedFrame> frames;

unsigned saveFrame()
{
	assert(frames.find(lastFrameId) == frames.end());
	frames.emplace(lastFrameId, SavedFrame{});
	return lastFrameId++;
}

void loadFramePre(size_t frame, unsigned id)
{
	frames.at(id).restorePre();
}

void loadFramePost(unsigned id)
{
	frames.at(id).restorePost();
}

void freeFrame(unsigned id)
{
	frames.erase(frames.find(id));
}

bool initGR()
{
	auto gr = LoadLibraryA("giuroll");

	if (!gr) {
		if (MessageBoxA(
			nullptr,
			"Netplay rollback not supported. This mod supports giuroll 0.6.14b, which wasn't found.\n"
			"If you are using it, make sure the line for giuroll is above the line for CustomWeathers in SWRSToys.ini.\n"
			"If you are using a rollback mod, playing online now will cause desyncs. Do you want to disable the mod now?",
			"CustomWeathers",
			MB_ICONWARNING | MB_YESNO
		) == IDYES)
			return false;
	} else {
		GiurollCallbacks cb{
			saveFrame,
			loadFramePre,
			loadFramePost,
			freeFrame
		};
		auto fct = GetProcAddress(gr, "addRollbackCb");

		if (!fct) {
			if (MessageBoxA(
				nullptr,
				"Netplay rollback not supported. This mod supports giuroll 0.6.14b, which wasn't found.\n"
				"A different (and not supported) giuroll version is in use. Please use version 0.6.14b, or otherwise one that is compatible.\n"
				"Playing online now will cause desyncs. Do you want to disable the mod now?",
				"CustomWeathers",
				MB_ICONWARNING | MB_YESNO
			) == IDYES)
				return false;
		} else
			reinterpret_cast<void (*)(const GiurollCallbacks *)>(fct)(&cb);
	}
	return true;
}

int (SokuLib::BattleManager::*og_battleMgrOnSayStart)();
int (SokuLib::BattleManager::*og_battleMgrAfterBlackScreen)();
int (SokuLib::BattleManager::*og_battleMgrMaybeOnProgress)();
int (SokuLib::BattleManager::*og_battleMgrOnRoundEnd)();
int (SokuLib::BattleManager::*og_battleMgrOnKO)();
int (SokuLib::BattleManager::*og_battleMgrOnGirlsTalk)();
int (SokuLib::BattleManager::*og_battleMgrUnknownFunction)();

void __fastcall handleBE3(SokuLib::v2::Player *This)
{
	This->renderInfos.zRotation = 0;
	This->FUN_0046d950();
	if (SokuLib::v2::GameDataManager::instance->players[currentIndex(This) + 2]->hp == 0)
		This->setAction(SokuLib::ACTION_BE3);
	else
		This->setAction(SokuLib::ACTION_BE2);
}

unsigned doBE3RetAddr = 0x487A53;

void __declspec(naked) checkBE3()
{
	__asm {
		MOV ECX, ESI
		CALL handleBE3
		JMP [doBE3RetAddr]
	}
}

void checkShock(SokuLib::v2::Player &chr, SokuLib::v2::Player &op, ChrInfo &info)
{
	if (op.timeStop)
		return;
	if (!chr.keyManager)
		return;
	if (chr.keyManager->keymapManager->input.select != 1)
		return;
	if (info.tagAntiBuffer)
		return;
	if (
		chr.frameState.actionId >= SokuLib::ACTION_RIGHTBLOCK_HIGH_SMALL_BLOCKSTUN &&
		chr.frameState.actionId < SokuLib::ACTION_AIR_GUARD &&
		chr.isOnGround() &&
		(info.meterReq == 0 && info.meter >= ASSIST_CARD_METER * 2 || info.burstCharges)
	) {
		if (info.meterReq == 0 && info.meter >= ASSIST_CARD_METER * 2)
			info.meter -= ASSIST_CARD_METER * 2;
		else
			info.burstCharges--;
		chr.setAction(SokuLib::ACTION_BOMB);
		info.tagAntiBuffer = 20;
		chr.createEffect(69, chr.position.x, chr.position.y + 120, 1, 1);
		return;
	}
	if (chr.frameState.actionId < SokuLib::ACTION_5A)
		return;
	if ((chr.frameState.actionId == SokuLib::ACTION_SYSTEM_CARD || chr.frameState.actionId == SokuLib::ACTION_BOMB) && chr.timeStop)
		return;
	if (chr.maxSpirit == 0)
		return;
	if (
		chr.frameState.actionId == SokuLib::ACTION_66A &&
		chr.frameState.sequenceId == 0 &&
		chr.frameState.currentFrame < 4 &&
		chr.inputData.keyInput.d >= chr.inputData.keyInput.a
	)
		return;
	if (SokuLib::activeWeather != SokuLib::WEATHER_SUNNY) {
		chr.maxSpirit -= 200;
		chr.timeWithBrokenOrb = 0;
		chr.currentSpirit = chr.maxSpirit;
	} else {
		for (int i = 0; i < 10; i++)
			chr.createEffect(200, chr.position.x, chr.position.y, chr.direction, 1);
		for (int i = 0; i < 10; i++)
			chr.createEffect(201, chr.position.x, chr.position.y, chr.direction, 1);
	}
	if (SokuLib::activeWeather == SokuLib::WEATHER_SUN_SHOWER) {
		for (int i = 0; i < 5; i++)
			chr.createEffect(200, chr.position.x, chr.position.y, chr.direction, 1);
		for (int i = 0; i < 5; i++)
			chr.createEffect(201, chr.position.x, chr.position.y, chr.direction, 1);
	}
	if (chr.currentSpirit < 200)
		chr.currentSpirit = 0;
	else
		chr.currentSpirit -= 200;
	if (chr.isOnGround()) {
		chr.gpShort[0] = 0;
		chr.setAction(SokuLib::ACTION_SYSTEM_CARD);
		chr.timeStop = 65;
		chr.grabInvulTimer = 60;
		chr.meleeInvulTimer = 60;
		chr.projectileInvulTimer = 60;
		SokuLib::playSEWaveBuffer(23);
		chr.createEffect(115, chr.position.x, chr.position.y + 120, 1, 1);
	} else {
		chr.setAction(SokuLib::ACTION_FALLING);
		chr.timeStop += 5;
		chr.hitStop += 5;
		chr.createEffect(69, chr.position.x, chr.position.y + 120, 1, 1);
	}
	chr.speed = {0, 0};
	chr.gravity = {0, 0};
	chr.renderInfos.scale = {1, 1};
	chr.renderInfos.xRotation = 0;
	chr.renderInfos.yRotation = 0;
	chr.renderInfos.zRotation = 0;
	SokuLib::camera.forceYCenter = false;
	SokuLib::camera.forceXCenter = false;
	SokuLib::camera.forceScale = false;
	if (op.frameState.actionId >= SokuLib::ACTION_GRABBED && op.frameState.actionId < 120) {
		op.gravity = {0, 0};
		if (op.position.y != 0) {
			op.setAction(SokuLib::ACTION_AIR_CRUSHED);
			op.gravity.y = 0.5;
		} else if (op.hp == 0) {
			op.setAction(SokuLib::ACTION_KNOCKED_DOWN);
			op.hitStop = 0;
		} else
			op.setAction(SokuLib::ACTION_GROUND_CRUSHED);
		if (op.hitStop > 15)
			op.hitStop = 15;
		op.speed = {0, 0};
		op.renderInfos.scale = {1, 1};
		op.renderInfos.xRotation = 0;
		op.renderInfos.yRotation = 0;
		op.renderInfos.zRotation = 0;
	}
}

void battleProcessCommon(SokuLib::BattleManager *This)
{
	auto players = (SokuLib::v2::Player**)((int)This + 0x0C);

	if (This->matchState == -1)
		return;
	if (SokuLib::checkKeyOneshot(DIK_F8, false, false, false))
		kill = !kill;
	if (SokuLib::checkKeyOneshot(DIK_F4, false, false, false))
		disp = !disp;
#ifdef _DEBUG
	if (SokuLib::checkKeyOneshot(DIK_F3, false, false, false))
		disp2 = !disp2;
	if (SokuLib::checkKeyOneshot(DIK_F2, false, false, false))
		disp3 = !disp3;
#endif

	if (This->matchState <= 2 || This->matchState == 4) {
		if (SokuLib::mainMode == SokuLib::BATTLE_MODE_PRACTICE && This->matchState == 2) {
			if (kill) {
				SokuLib::v2::GameDataManager::instance->players[2]->hp = 0;
				SokuLib::v2::GameDataManager::instance->players[3]->hp = 0;
			} else {
				SokuLib::v2::GameDataManager::instance->players[2]->hp = SokuLib::v2::GameDataManager::instance->players[2]->maxHP;
				SokuLib::v2::GameDataManager::instance->players[3]->hp = SokuLib::v2::GameDataManager::instance->players[3]->maxHP;
			}
		}
		SokuLib::camera.p1X = &SokuLib::v2::GameDataManager::instance->players[0]->position.x;
		SokuLib::camera.p2X = &SokuLib::v2::GameDataManager::instance->players[0]->position.x;
		SokuLib::camera.p1Y = &SokuLib::v2::GameDataManager::instance->players[0]->position.y;
		SokuLib::camera.p2Y = &SokuLib::v2::GameDataManager::instance->players[0]->position.y;
		for (int i = 1; i < 4; i++) {
			if (i >= 2 && SokuLib::v2::GameDataManager::instance->players[i]->renderInfos.yRotation == 90)
				continue;
			if (i >= 2 && SokuLib::v2::GameDataManager::instance->players[i]->hp == 0)
				continue;
			if (*SokuLib::camera.p1X > SokuLib::v2::GameDataManager::instance->players[i]->position.x)
				SokuLib::camera.p1X = &SokuLib::v2::GameDataManager::instance->players[i]->position.x;
			if (*SokuLib::camera.p2X < SokuLib::v2::GameDataManager::instance->players[i]->position.x)
				SokuLib::camera.p2X = &SokuLib::v2::GameDataManager::instance->players[i]->position.x;
			if (*SokuLib::camera.p1Y > SokuLib::v2::GameDataManager::instance->players[i]->position.y)
				SokuLib::camera.p1Y = &SokuLib::v2::GameDataManager::instance->players[i]->position.y;
			if (*SokuLib::camera.p2Y < SokuLib::v2::GameDataManager::instance->players[i]->position.y)
				SokuLib::camera.p2Y = &SokuLib::v2::GameDataManager::instance->players[i]->position.y;
		}
	}
	if (!SokuLib::menuManager.empty() && SokuLib::sceneId == SokuLib::SCENE_BATTLE)
		return;

	for (int i = 0; i < 2; i++) {
		auto &info = (&currentChr.first)[i];
		auto keys = SokuLib::v2::GameDataManager::instance->players[i]->keyManager;

		if (info.tagAntiBuffer)
			info.tagAntiBuffer--;
		if (SokuLib::v2::GameDataManager::instance->players[i + 2]->hp == 0) {
			auto mate = SokuLib::v2::GameDataManager::instance->players[i + 2];

			if (info.maxCd == 0)
				info.maxCd = 10;
			info.cd = info.maxCd;
			mate->untech = 60;
			mate->renderInfos.yRotation = 0;
			if (
				mate->frameState.actionId < SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN ||
				mate->frameState.actionId >= SokuLib::ACTION_RIGHTBLOCK_HIGH_SMALL_BLOCKSTUN
			) {
				mate->speed.y = 15;
				mate->gravity.y = 1;
				mate->setAction(SokuLib::ACTION_AIR_HIT_MEDIUM_HITSTUN);
			}
			checkShock(
				*SokuLib::v2::GameDataManager::instance->players[i],
				*SokuLib::v2::GameDataManager::instance->players[!i],
				info
			);
		} else if (SokuLib::v2::GameDataManager::instance->players[i]->hp == 0 && This->matchState <= 2) {
			info.deathTag = true;
			info.activePose = 0;
			goto swap;
		} else if (
			keys &&
			(
				keys->keymapManager->input.d && keys->keymapManager->input.d < 10 ||
				keys->keymapManager->input.changeCard && keys->keymapManager->input.changeCard < 10
			) &&
			!info.tagAntiBuffer &&
			keys->keymapManager->input.select && keys->keymapManager->input.select < 10 &&
			(info.cd == 0 || !info.started) &&
			!info.tagging && (
				This->matchState != 2 ||
				SokuLib::v2::GameDataManager::instance->players[i]->frameState.actionId < SokuLib::ACTION_STAND_GROUND_HIT_SMALL_HITSTUN ||
				SokuLib::v2::GameDataManager::instance->players[i]->frameState.actionId > SokuLib::ACTION_NEUTRAL_TECH ||
				(info.meterReq == 0 && info.meter >= SLOW_TAG_COST * ASSIST_CARD_METER || info.burstCharges)
			)
		) {
			info.deathTag = false;
		swap:
			auto arr = *(SokuLib::v2::Player ***)(*(int *)SokuLib::ADDR_GAME_DATA_MANAGER + 0x40);
			auto old = SokuLib::v2::GameDataManager::instance->players[i + 2];

			info.tagAntiBuffer = 15;
			SokuLib::v2::GameDataManager::instance->players[i + 2] = SokuLib::v2::GameDataManager::instance->players[i];
			players[i + 2] = SokuLib::v2::GameDataManager::instance->players[i];
			SokuLib::v2::GameDataManager::instance->activePlayers[i + 2] = SokuLib::v2::GameDataManager::instance->players[i];
			SokuLib::v2::GameDataManager::instance->players[i] = old;
			players[i] = old;
			SokuLib::v2::GameDataManager::instance->activePlayers[i] = old;
			info.tagging = true;
			SokuLib::v2::GameDataManager::instance->players[i]->gameData.ally = SokuLib::v2::GameDataManager::instance->players[i];
			SokuLib::v2::GameDataManager::instance->players[i + 2]->gameData.ally = SokuLib::v2::GameDataManager::instance->players[i];
			SokuLib::v2::GameDataManager::instance->players[i]->comboDamage = SokuLib::v2::GameDataManager::instance->players[i + 2]->comboDamage;
			SokuLib::v2::GameDataManager::instance->players[i]->comboLimit = SokuLib::v2::GameDataManager::instance->players[i + 2]->comboLimit;
			SokuLib::v2::GameDataManager::instance->players[i]->comboRate = SokuLib::v2::GameDataManager::instance->players[i + 2]->comboRate;
			SokuLib::v2::GameDataManager::instance->players[i]->comboCount = SokuLib::v2::GameDataManager::instance->players[i + 2]->comboCount;
			SokuLib::v2::GameDataManager::instance->players[i + 2]->comboDamage = 0;
			SokuLib::v2::GameDataManager::instance->players[i + 2]->comboLimit = 0;
			SokuLib::v2::GameDataManager::instance->players[i + 2]->comboRate = 0;
			SokuLib::v2::GameDataManager::instance->players[i + 2]->comboCount = 0;
			SokuLib::v2::GameDataManager::instance->players[!i]->gameData.opponent = SokuLib::v2::GameDataManager::instance->players[i];
			SokuLib::v2::GameDataManager::instance->players[!i + 2]->gameData.opponent = SokuLib::v2::GameDataManager::instance->players[i];
			initTagAnim(
				info,
				SokuLib::v2::GameDataManager::instance->players[i + 2]->characterIndex,
				*SokuLib::v2::GameDataManager::instance->players[i],
				*SokuLib::v2::GameDataManager::instance->players[i + 2],
				loadouts[originalIndex(SokuLib::v2::GameDataManager::instance->players[i + 2])],
				This->matchState
			);
			hud1->playerHUD[i].player = SokuLib::v2::GameDataManager::instance->players[i];
			hud2.playerHUD[i].player = SokuLib::v2::GameDataManager::instance->players[i + 2];
			std::swap(hud1->playerHUD[i].lifebarYellowValue, hud2.playerHUD[i].lifebarYellowValue);
			std::swap(hud1->playerHUD[i].lifebarRedValue, hud2.playerHUD[i].lifebarRedValue);
			for (auto o : hud1->playerHUD[i].orbsGauge)
				((short **)(o->gauge.value))[1] = &SokuLib::v2::GameDataManager::instance->players[i]->currentSpirit;
			for (auto o : hud1->playerHUD[i].orbsCrushGauge)
				((short **)(o->gauge.value))[1] = &SokuLib::v2::GameDataManager::instance->players[i]->timeWithBrokenOrb;
			for (auto o : hud1->playerHUD[i].cardGauge)
				((short **)(o->gauge.value))[1] = &SokuLib::v2::GameDataManager::instance->players[i]->handInfo.cardGauge;
		}
	}
	for (int i = 0; i < 4; i++)
		currentIndex(SokuLib::v2::GameDataManager::instance->players[i]) = i;

	for (int i = 0; i < 2; i++) {
		if (SokuLib::v2::GameDataManager::instance->players[i]->handInfo.hand.empty())
			continue;

		auto &elems = SokuLib::v2::GameDataManager::instance->players[i]->handInfo.hand[0];
		auto cost = elems.cost;

		if (players[i]->weatherId == SokuLib::WEATHER_CLOUDY)
			cost--;
		if (cost < 1)
			cost = 1;
		if (SokuLib::v2::GameDataManager::instance->players[i]->handInfo.hand.size() >= cost) {
			highlightAnimation[i]++;
			highlightAnimation[i] %= 20;
		}
	}
	for (int i = 2; i < 4; i++) {
		auto &info = (&currentChr.first)[i - 2];
		auto &elems = loadedData[(&currentChr.first)[i - 2].chr][info.loadoutIndex];
		auto cost = elems.shownCost;

		if (players[i]->weatherId == SokuLib::WEATHER_CLOUDY)
			cost--;
		if (cost < 1)
			cost = 1;
		if (info.meter >= cost * ASSIST_CARD_METER) {
			highlightAnimation[i]++;
			highlightAnimation[i] %= 20;
		}
	}
	if (players[0]->timeStop || players[1]->timeStop) {
		if (SokuLib::v2::GameDataManager::instance->players[2]->timeStop)
			updateObject(players[0], SokuLib::v2::GameDataManager::instance->players[2], currentChr.first);
		if (SokuLib::v2::GameDataManager::instance->players[3]->timeStop)
			updateObject(players[1], SokuLib::v2::GameDataManager::instance->players[3], currentChr.second);
	} else {
		if (!SokuLib::v2::GameDataManager::instance->players[3]->timeStop)
			updateObject(players[0], SokuLib::v2::GameDataManager::instance->players[2], currentChr.first);
		if (!SokuLib::v2::GameDataManager::instance->players[2]->timeStop)
			updateObject(players[1], SokuLib::v2::GameDataManager::instance->players[3], currentChr.second);
	}
	assisterAttacks(players[0], SokuLib::v2::GameDataManager::instance->players[2], currentChr.first,  loadedData[currentChr.first.chr][currentChr.first.loadoutIndex]);
	assisterAttacks(players[1], SokuLib::v2::GameDataManager::instance->players[3], currentChr.second, loadedData[currentChr.second.chr][currentChr.second.loadoutIndex]);

	if (players[2]->timeStop > 1)
		players[0]->timeStop = max(players[0]->timeStop + 1, 2);
	else if (players[0]->timeStop > 1)
		players[2]->timeStop = max(players[2]->timeStop + 1, 2);
	if (players[3]->timeStop > 1)
		players[1]->timeStop = max(players[1]->timeStop + 1, 2);
	else if (players[1]->timeStop > 1)
		players[3]->timeStop = max(players[3]->timeStop + 1, 2);

	if (This->matchState == 1) {
		currentChr.first.cd = 0;
		currentChr.second.cd = 0;
		if (!anim) {
			anim = true;
			initSkillUpgrade(
				currentChr.first,
				assists.first.character,
				*SokuLib::v2::GameDataManager::instance->players[2],
				*SokuLib::v2::GameDataManager::instance->players[0],
				loadouts[originalIndex(SokuLib::v2::GameDataManager::instance->players[2])]
			);
			initSkillUpgrade(
				currentChr.second,
				assists.second.character,
				*SokuLib::v2::GameDataManager::instance->players[3],
				*SokuLib::v2::GameDataManager::instance->players[1],
				loadouts[originalIndex(SokuLib::v2::GameDataManager::instance->players[3])]
			);
		}
	}

	if (This->matchState == 3) {
		players[0]->kdAnimationFinished = players[0]->kdAnimationFinished || players[2]->kdAnimationFinished;
		players[1]->kdAnimationFinished = players[1]->kdAnimationFinished || players[3]->kdAnimationFinished;
	} else {
		players[0]->kdAnimationFinished = players[0]->kdAnimationFinished && players[2]->kdAnimationFinished;
		players[1]->kdAnimationFinished = players[1]->kdAnimationFinished && players[3]->kdAnimationFinished;
	}
	players[0]->roundsWins = players[0]->roundsWins && players[2]->roundsWins;
	players[1]->roundsWins = players[1]->roundsWins && players[3]->roundsWins;
	players[2]->roundsWins = players[0]->roundsWins;
	players[3]->roundsWins = players[1]->roundsWins;
	players[2]->kdAnimationFinished = players[0]->kdAnimationFinished;
	players[3]->kdAnimationFinished = players[1]->kdAnimationFinished;
	players[0]->score = max(players[0]->score, players[2]->score);
	players[1]->score = max(players[1]->score, players[3]->score);
	players[2]->score = max(players[0]->score, players[2]->score);
	players[3]->score = max(players[1]->score, players[3]->score);
}

int __fastcall CBattleManager_OnSayStart(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrOnSayStart)();
}
int __fastcall CBattleManager_AfterBlackScreen(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrAfterBlackScreen)();
}
int __fastcall CBattleManager_MaybeOnProgress(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrMaybeOnProgress)();
}
int __fastcall CBattleManager_OnRoundEnd(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrOnRoundEnd)();
}
int __fastcall CBattleManager_OnKO(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrOnKO)();
}
int __fastcall CBattleManager_OnGirlsTalk(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrOnGirlsTalk)();
}
int __fastcall CBattleManager_UnknownFunction(SokuLib::BattleManager *This)
{
	battleProcessCommon(This);
	return (This->*og_battleMgrUnknownFunction)();
}

void *(*og_getSaveDataMgr)();

void *onLoadingDone()
{
	if (spawned)
		init = false;
	anim = false;
	spawned = true;

	std::map<std::pair<unsigned, unsigned>, bool> loaded;

	loadAssistData(SokuLib::leftChar, 0);
	if (SokuLib::leftChar != SokuLib::rightChar || loadouts[0] != loadouts[1])
		loadAssistData(SokuLib::rightChar, 1);
	loaded[{SokuLib::leftChar, loadouts[0]}] = true;
	loaded[{SokuLib::rightChar, loadouts[1]}] = true;
	for (int i = 0; i < 2; i++) {
		auto &chr = (&assists.first)[i];

		if (loaded[{chr.character, loadouts[i + 2]}])
			continue;
		loaded[{chr.character, loadouts[i + 2]}] = true;
		loadAssistData(chr.character, i + 2);
	}
	currentChr.first = ChrInfo();
	currentChr.first.meter = 0;
	currentChr.first.meterReq = 0;
	currentChr.first.burstCharges = START_BURST_CHARGES;
	currentChr.first.loadoutIndex = loadouts[2];
	currentChr.first.chr = assists.first.character;
	currentChr.second = ChrInfo();
	currentChr.second.meter = 0;
	currentChr.second.meterReq = 0;
	currentChr.second.burstCharges = START_BURST_CHARGES;
	currentChr.second.loadoutIndex = loadouts[3];
	currentChr.second.chr = assists.second.character;

	SokuLib::v2::Player** players = (SokuLib::v2::Player**)((int)&SokuLib::getBattleMgr() + 0xC);

	assists.first.keyManager = SokuLib::v2::GameDataManager::instance->players[0]->keyManager;
	assists.second.keyManager = SokuLib::v2::GameDataManager::instance->players[1]->keyManager;

	puts("Not spawned. Loading both assisters");
	puts("Loading character 1");
	((void (__thiscall *)(SokuLib::v2::GameDataManager*, int, SokuLib::PlayerInfo &))0x46da40)(SokuLib::v2::GameDataManager::instance, 2, assists.first);
	(*(void (__thiscall **)(SokuLib::v2::Player *))(*(int *)SokuLib::v2::GameDataManager::instance->players[2] + 0x44))(SokuLib::v2::GameDataManager::instance->players[2]);
	players[2] = SokuLib::v2::GameDataManager::instance->players[2];
	SokuLib::v2::GameDataManager::instance->enabledPlayers[2] = true;

	puts("Loading character 2");
	((void (__thiscall *)(SokuLib::v2::GameDataManager*, int, SokuLib::PlayerInfo &))0x46da40)(SokuLib::v2::GameDataManager::instance, 3, assists.second);
	(*(void (__thiscall **)(SokuLib::v2::Player *))(*(int *)SokuLib::v2::GameDataManager::instance->players[3] + 0x44))(SokuLib::v2::GameDataManager::instance->players[3]);
	players[3] = SokuLib::v2::GameDataManager::instance->players[3];
	SokuLib::v2::GameDataManager::instance->enabledPlayers[3] = true;
	init = false;
	printf("%p %p\n", SokuLib::v2::GameDataManager::instance->players[2], SokuLib::v2::GameDataManager::instance->players[3]);
	// Init
	//if (hudInit)
	//	((void (__thiscall *)(CInfoManager *, bool))*hud2.vtable)(&hud2, 0);
	hudInit = true;
	initHud();

	puts("Init assisters");
	currentChr.first.cd = 0;
	currentChr.second.cd = 0;
	SokuLib::v2::GameDataManager::instance->activePlayers.push_back(SokuLib::v2::GameDataManager::instance->players[2]);
	SokuLib::v2::GameDataManager::instance->activePlayers.push_back(SokuLib::v2::GameDataManager::instance->players[3]);
	SokuLib::v2::GameDataManager::instance->players[2]->gameData.opponent = SokuLib::v2::GameDataManager::instance->players[1];
	SokuLib::v2::GameDataManager::instance->players[3]->gameData.opponent = SokuLib::v2::GameDataManager::instance->players[0];
	SokuLib::v2::GameDataManager::instance->players[3]->gameData.ally = SokuLib::v2::GameDataManager::instance->players[1];
	SokuLib::v2::GameDataManager::instance->players[2]->gameData.ally = SokuLib::v2::GameDataManager::instance->players[0];
	SokuLib::v2::GameDataManager::instance->players[2]->renderInfos.yRotation = 90;
	SokuLib::v2::GameDataManager::instance->players[3]->renderInfos.yRotation = 90;
	for (int i = 0; i < 4; i++) {
		originalIndex(SokuLib::v2::GameDataManager::instance->players[i]) = i;
		currentIndex(SokuLib::v2::GameDataManager::instance->players[i]) = i;
	}
	init = true;
	chrLoadingIndex = 0;
	return og_getSaveDataMgr();
}

void __fastcall turnAroundHitBlock(SokuLib::CharacterManager *This, SokuLib::ObjectManager *op)
{
	float fVar1;
	float fVar2;
	char cVar3;
	SokuLib::CharacterManager *pPVar4;

	cVar3 = This->objectBase.direction;
	pPVar4 = op->owner;
	if (This->objectBase.position.x < pPVar4->objectBase.position.x)
		This->objectBase.direction = SokuLib::RIGHT;
	else if (This->objectBase.position.x > pPVar4->objectBase.position.x)
		This->objectBase.direction = SokuLib::LEFT;
}

void __declspec(naked) turnAroundHitBlock_hook()
{
	__asm {
		MOV EDX, EDI
		JMP turnAroundHitBlock
	}
}

void __declspec(naked) setGrazeFlags()
{
	__asm {
		CMP dword ptr [EDI + 0x190], 0
		JNZ _ret
		MOV dword ptr [EDI + 0x190], 0x6
		_ret:
		RET
	}
}

void __declspec(naked) swapComboCtrs()
{
	__asm {
		POP        EDX

		MOV        EAX, dword ptr [ESP + 0x14]
		LEA        EAX, [EAX + ESI * 0x4]
		MOV        ECX, 0x8985E4
		ADD        EAX, dword ptr [ECX]
		MOV        EAX, dword ptr [ESP + EAX * 0x1 + 0x1c]
		MOV        dword ptr [ESP + ESI * 0x4 + 0x1c], EAX

		MOV        EAX, dword ptr [ESP + 0x14]
		PUSH       ESI
		XOR        ESI, 1
		LEA        EAX, [EAX + ESI * 0x4]
		MOV        ESI, 0x8985E4
		ADD        EAX, dword ptr [ESI]
		POP        ESI
		MOV        EAX, dword ptr [ESP + EAX * 0x1 + 0x1c]

		MOV        ECX, dword ptr [ESP + 0x18]
		PUSH       EAX
		PUSH       ECX
		MOV        ECX, EDI
		JMP        EDX
	}
}

void __declspec(naked) getCharacterIndexResetHealth_hook()
{
	__asm {
		MOV ESI, 0x8985E4 //CBattleManagerPtr
		MOV ESI, [ESI]
		MOV ESI, dword ptr [ESI + EDI * 0x4 + 0xC]
		MOV ECX, ESI
		RET
	}
}

const void *parseExtraChrsGameMatch(SokuLib::PlayerMatchData *ptr)
{
	auto *infos = &assists.first;
	const unsigned char *ret;

	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT) {
		infos++;
		ptr = reinterpret_cast<SokuLib::PlayerMatchData *>(ptr->getEndPtr());
		infos->effectiveDeck.clear();
		for (unsigned i = 0; i < ptr->deckSize; i++)
			infos->effectiveDeck.push_back(ptr->cards[i]);
		netplayDeck[3] = infos->effectiveDeck;
		ret = ptr->getEndPtr();
	} else if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSSERVER) {
		infos->character = static_cast<SokuLib::Character>(ptr->character);
		infos->palette = ptr->skinId;
		infos->deck = ptr->deckId;
		infos->effectiveDeck.clear();
		for (unsigned i = 0; i < ptr->deckSize; i++)
			infos->effectiveDeck.push_back(ptr->cards[i]);
		netplayDeck[2] = infos->effectiveDeck;
		ptr = reinterpret_cast<SokuLib::PlayerMatchData *>(ptr->getEndPtr());
		infos++;
		infos->character = static_cast<SokuLib::Character>(ptr->character);
		infos->palette = ptr->skinId;
		infos->deck = ptr->deckId;

		ret = ptr->getEndPtr();
		loadouts[0] = ret[0];
		loadouts[1] = ret[1];
		loadouts[2] = ret[2];
		loadouts[3] = ret[3];
	} else {
		for (int j = 0; j < 2; j++) {
			infos->character = static_cast<SokuLib::Character>(ptr->character);
			infos->palette = ptr->skinId;
			infos->deck = ptr->deckId;
			infos->effectiveDeck.clear();
			for (unsigned i = 0; i < ptr->deckSize; i++)
				infos->effectiveDeck.push_back(ptr->cards[i]);
			netplayDeck[2 + j] = infos->effectiveDeck;
			ptr = reinterpret_cast<SokuLib::PlayerMatchData *>(ptr->getEndPtr());
			infos++;
		}

		ret = reinterpret_cast<uint8_t *>(ptr);
		loadouts[0] = ret[0];
		loadouts[1] = ret[1];
		loadouts[2] = ret[2];
		loadouts[3] = ret[3];
	}
	return ret + 4;
}

void __declspec(naked) parseExtraChrsGameMatch_hook()
{
	__asm {
		PUSH ESI
		CALL parseExtraChrsGameMatch
		ADD ESP, 4
		MOV ESI, EAX
		MOVZX EAX, byte ptr [ESI]
		MOV byte ptr [EBX + 0x3a], AL
		RET
	}
}

void *__fastcall addExtraChrsGameMatch(void *packet)
{
	auto data = reinterpret_cast<SokuLib::PlayerMatchData *>(packet);
	auto *infos = &assists.first;
	auto *deck = &netplayDeck[2];

	for (int j = 0; j < 2; j++) {
		data->character = static_cast<SokuLib::CharacterPacked>(infos->character);
		data->skinId = infos->palette;
		data->deckId = infos->deck;
		data->deckSize = deck->size();
		for (unsigned i = 0; i < data->deckSize; i++)
			data->cards[i] = (*deck)[i];
		data = reinterpret_cast<SokuLib::PlayerMatchData *>(data->getEndPtr());
		infos++;
		deck++;
	}

	auto p = reinterpret_cast<uint8_t *>(data);

	p[0] = loadouts[0];
	p[1] = loadouts[1];
	p[2] = loadouts[2];
	p[3] = loadouts[3];
	//}
	return reinterpret_cast<char *>(data) + 4;
}

void __declspec(naked) addExtraChrsGameMatch_hook()
{
	__asm {
		PUSH ECX
		MOV ECX, EDI
		CALL addExtraChrsGameMatch
		MOV EDI, EAX
		POP ECX
		MOV AL, byte ptr [ECX + 0x3A]
		MOV byte ptr [EDI], AL
		RET
	}
}

int (__stdcall *og_recvfrom)(SOCKET s, char * buf, int len, int flags, sockaddr * from, int * fromlen);
int (__stdcall *og_sendto)(SOCKET s, char * buf, int len, int flags, sockaddr * to, int tolen);

std::mutex m;
int __stdcall my_recvfrom(SOCKET s, char * buf, int len, int flags, sockaddr * from, int * fromlen)
{
	int ret = og_recvfrom(s, buf, len, flags, from, fromlen);
	auto packet = (SokuLib::Packet *)buf;

	displayPacket(packet, "RECV: ");
	if (SokuLib::mainMode != SokuLib::BATTLE_MODE_VSSERVER)
		return ret;
	if (packet->type != SokuLib::CLIENT_GAME && packet->type != SokuLib::HOST_GAME)
		return ret;
	if (packet->game.event.type == SokuLib::GAME_MATCH) {
		auto packet_ = (PacketGameMatchEvent *)buf;

		generateFakeDecks(static_cast<SokuLib::Character>((*packet_)[1].character), static_cast<SokuLib::Character>((*packet_)[3].character));
	}
	return ret;
}

int __stdcall my_sendto(SOCKET s, char * buf, int len, int flags, sockaddr * to, int tolen)
{
	auto packet = reinterpret_cast<SokuLib::Packet *>(buf);

	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSWATCH) {
		displayPacket(packet, "SEND: ");
		return og_sendto(s, buf, len, flags, to, tolen);
	}
	if (packet->type != SokuLib::CLIENT_GAME && packet->type != SokuLib::HOST_GAME) {
		displayPacket(packet, "SEND: ");
		return og_sendto(s, buf, len, flags, to, tolen);
	}

	bool needDelete = false;

	if (packet->game.event.type == SokuLib::GAME_MATCH) {
		auto pack = reinterpret_cast<PacketGameMatchEvent *>(buf);
		auto index = (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSCLIENT ? 0 : 1);

		generateFakeDecks();
		for (; index < 2; index += 2)
			if (fakeDeck[index])
				memcpy(pack->operator[](index).cards, fakeDeck[index]->data(), fakeDeck[index]->size() * sizeof(*fakeDeck[index]->data()));
			else //We just send an invalid deck over if we want no decks
				memset(pack->operator[](index).cards, 0, 40);
	}
	displayPacket(packet, "SEND: ");
	return og_sendto(s, buf, len, flags, to, tolen);
}

void onMeterGained(SokuLib::v2::Player *player, int meter)
{
	gainMeter((&currentChr.first)[currentIndex(player)], meter * ASSIST_METER_CONVERSION);
}

void __declspec(naked) onMeterGained_hook()
{
	__asm {
		MOV AL, [ECX + 0x14E]
		CMP AL, 2
		JL lower
		MOV ESI, [ECX + 0x16C]
	lower:
		PUSH [ESP + 0xC]
		PUSH ESI
		CALL onMeterGained
		ADD ESP, 8
		FLD dword ptr [ESI + 0x54C]
		RET
	}
}

void __declspec(naked) onMeterGainedHit_hook()
{
	__asm {
		MOV AL, [ECX + 0x14E]
		CMP AL, 2
		JL lower
		MOV ECX, [ECX + 0x16C]
	lower:
		PUSH [ESP + 0x8]
		PUSH ECX
		CALL onMeterGained
		POP ECX
		ADD ESP, 4
		CMP byte ptr [ECX + 0x7CC],0x2
		RET
	}
}

void __declspec(naked) takeOpponentCorrection()
{
	__asm {
		MOV ESI, dword ptr [ESI + 0x170]
		MOV ESI, dword ptr [ESI + 0x16C]
		MOV AL, byte ptr [ESI + 0x4AD]
		RET
	}
}

const double profileNameY = 57;
const double profileNameLeftX = 186;
const double profileNameRightX = 640 - profileNameLeftX;

const float sanaeKanakoLeftGui = 96;      // (+40)
const float sanaeKanakoRightGui = 544;    // (-40)
const float sanaeKanakoTopGui = 366;      // (+10)
const float sanaeKanakoLeftBarGui = 122;  // (+40)
const float sanaeKanakoRightBarGui = 518; // (-40)
const float sanaeKanakoTopBarGui = 412;   // (+10)

const float sanaeSuwakoLeftGui = 162;     // (+10)
const float sanaeSuwakoRightGui = 478;    // (-10)
const float sanaeSuwakoLeftBarGui = 187;  // (+10)
const float sanaeSuwakoRightBarGui = 453; // (-10)

unsigned fixupSanaeKanakoCross_ret = 0x76067D;
void __declspec(naked) fixupSanaeKanakoCross()
{
	__asm {
		FLD        dword ptr [sanaeKanakoLeftBarGui]
		FSTP       dword ptr [ESI + 0xEC]
		FLD        dword ptr [sanaeKanakoTopBarGui]
		FSTP       dword ptr [ESI + 0xF0]
		JMP        [fixupSanaeKanakoCross_ret]
	}
}

unsigned fixupSanaeSuwakoBar_ret = 0x760361;
void __declspec(naked) fixupSanaeSuwakoBar()
{
	__asm {
		FLD        dword ptr [sanaeSuwakoLeftBarGui]
		FSTP       dword ptr [ESI + 0xEC]
		MOV        EAX, 0x85EEE8
		FLD        dword ptr [EAX]
		FSTP       dword ptr [ESI + 0xF0]
		JMP        [fixupSanaeSuwakoBar_ret]
	}
}

unsigned fixupSanaeSuwakoBarDisabled_ret = 0x7604AE;
void __declspec(naked) fixupSanaeSuwakoBarDisabled()
{
	__asm {
		MOV        EAX, 0x85EEE8
		FLD        dword ptr [EAX]
		FSTP       dword ptr [ESI + 0xF0]
		JMP        [fixupSanaeSuwakoBarDisabled_ret]
	}
}

unsigned fixupSanaeSuwakoCrossDisabled_ret = 0x76062C;
void __declspec(naked) fixupSanaeSuwakoCrossDisabled()
{
	__asm {
		FLD        dword ptr [sanaeSuwakoRightBarGui]
		FSTP       dword ptr [ESI + 0xEC]
		MOV        EAX, 0x85EEE8
		FLD        dword ptr [EAX]
		FSTP       dword ptr [ESI + 0xF0]
		JMP        [fixupSanaeSuwakoCrossDisabled_ret]
	}
}

extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule) {
	DWORD old;

#ifdef _DEBUG
	FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif

	random.seed(time(nullptr));
	if (!initGR())
		return false;
#ifdef _DEBUG
	time_t t = time(nullptr);
	struct tm *m = localtime(&t);
	char buffer[128];

	strftime(buffer, sizeof(buffer), "packets_%Y_%m_%d_%H_%M_%S.log", m);
	logStream.open(buffer);
#endif
	GetModuleFileName(hMyModule, modFolder, 1024);
	PathRemoveFileSpec(modFolder);
	puts("Hello");
	// DWORD old;
	loadSoku2Config();
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	s_originalBattleMgrOnRender          = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRender, CBattleManager_OnRender);
	og_battleMgrOnSayStart               = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onSayStart, CBattleManager_OnSayStart);
	og_battleMgrAfterBlackScreen         = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.afterBlackScreen, CBattleManager_AfterBlackScreen);
	og_battleMgrMaybeOnProgress          = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.maybeOnProgress, CBattleManager_MaybeOnProgress);
	og_battleMgrOnRoundEnd               = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onRoundEnd, CBattleManager_OnRoundEnd);
	og_battleMgrOnKO                     = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onKO, CBattleManager_OnKO);
	og_battleMgrOnGirlsTalk              = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.onGirlsTalk, CBattleManager_OnGirlsTalk);
	og_battleMgrUnknownFunction          = SokuLib::TamperDword(&SokuLib::VTable_BattleManager.unknownFunction, CBattleManager_UnknownFunction);
	s_originalSelectOnProcess            = SokuLib::TamperDword(&SokuLib::VTable_Select.update, CSelect_OnProcess);
	s_originalSelectOnRender             = SokuLib::TamperDword(&SokuLib::VTable_Select.onRender, CSelect_OnRender);
	s_originalSelectClientOnProcess      = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.update, CSelectCL_OnProcess);
	s_originalSelectClientOnRender       = SokuLib::TamperDword(&SokuLib::VTable_SelectClient.onRender, CSelectCL_OnRender);
	s_originalSelectServerOnProcess      = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.update, CSelectSV_OnProcess);
	s_originalSelectServerOnRender       = SokuLib::TamperDword(&SokuLib::VTable_SelectServer.onRender, CSelectSV_OnRender);
	s_originalTitleOnProcess             = SokuLib::TamperDword(&SokuLib::VTable_Title.onRender, CTitle_OnProcess);
	s_originalCProfileDeckEdit_OnProcess = SokuLib::TamperDword(&SokuLib::VTable_ProfileDeckEdit.onProcess, CProfileDeckEdit_OnProcess);
	s_originalCProfileDeckEdit_OnRender  = SokuLib::TamperDword(&SokuLib::VTable_ProfileDeckEdit.onRender, CProfileDeckEdit_OnRender);
	s_originalCProfileDeckEdit_Destructor= SokuLib::TamperDword(&SokuLib::VTable_ProfileDeckEdit.onDestruct, CProfileDeckEdit_Destructor);
	ogHudRender = (int (__thiscall *)(void *))SokuLib::TamperDword(0x85b544, onHudRender);
	for (int i = 0; i < 32; i++)
		if (i & 15)
			((unsigned char *)0x858B80)[i] ^= versionMask[i % sizeof(versionMask)];
	og_sendto    = SokuLib::TamperDword(&SokuLib::DLL::ws2_32.sendto, my_sendto);
	og_recvfrom  = SokuLib::TamperDword(&SokuLib::DLL::ws2_32.recvfrom, my_recvfrom);
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	// Force deck icon to be hidden in character select
	*(unsigned char *)0x4210e2 = 0xEB;
	// Force deck icon to be hidden in deck construction
	memset((void *)0x0044E4ED, 0x90, 35);
	SokuLib::TamperNearJmpOpr(0x450230, CProfileDeckEdit_SwitchCurrentDeck);
	s_originalDrawGradiantBar = reinterpret_cast<void (*)(float, float, float)>(SokuLib::TamperNearJmpOpr(0x44E4C8, drawGradiantBar));
	og_CProfileDeckEdit_Init = SokuLib::union_cast<SokuLib::ProfileDeckEdit *(SokuLib::ProfileDeckEdit::*)(int, int, SokuLib::Sprite *)>(
		SokuLib::TamperNearJmpOpr(0x0044d529, CProfileDeckEdit_Init)
	);

	*(char *)0x4879FB = 0x74;
	*(char *)0x4879FC = 0x39;
	*(char *)0x4879FD = 0x7F;
	*(char *)0x4879FE = 0x33;
	*(char *)0x487A35 = 0x74;
	SokuLib::TamperNearJmp(0x487AAA, checkBE3);

	*(const char **)0x47DEE5 = battleUpperPath1;

	*(const double **)0x47D9B0 = &profileNameY;
	*(const double **)0x47D8EB = &profileNameY;
	*(const double **)0x47D9DD = &profileNameLeftX;
	*(const double **)0x47D91A = &profileNameLeftX;
	*(const double **)0x47DB57 = &profileNameY;
	*(const double **)0x47DA96 = &profileNameY;
	*(const double **)0x47DB84 = &profileNameRightX;
	*(const double **)0x47DAC5 = &profileNameRightX;

	// Sanae GUI fix
	// Kanako portrait
	*(const float **)0x7602BF = &sanaeKanakoLeftGui;
	*(const float **)0x7602CB = &sanaeKanakoTopGui;
	*(const float **)0x7602E0 = &sanaeKanakoRightGui;
	*(const float **)0x7602EC = &sanaeKanakoTopGui;
	// Kanako enabled bar
	*(const float **)0x7603E5 = &sanaeKanakoLeftBarGui;
	*(const float **)0x7603F1 = &sanaeKanakoTopBarGui;
	*(const float **)0x760409 = &sanaeKanakoRightBarGui;
	*(const float **)0x760415 = &sanaeKanakoTopBarGui;
	// Kanako disabled bar
	*(const float **)0x760498 = &sanaeKanakoLeftBarGui;
	*(const float **)0x7604A4 = &sanaeKanakoTopBarGui;
	*(const float **)0x7604BC = &sanaeKanakoRightBarGui;
	*(const float **)0x7604C8 = &sanaeKanakoTopBarGui;
	// Kanako disabled cross
	SokuLib::TamperNearJmp(0x76060C, fixupSanaeKanakoCross);
	*(const float **)0x760616 = &sanaeKanakoRightBarGui;
	*(const float **)0x760622 = &sanaeKanakoTopBarGui;
	// Suwako bars
	SokuLib::TamperNearJmp(0x760508, fixupSanaeSuwakoBar);
	// Suwako portrait
	*(const float **)0x76034B = &sanaeSuwakoLeftGui;
	*(const float **)0x76036C = &sanaeSuwakoRightGui;
	// Suwako enabled bar
	*(const float **)0x760515 = &sanaeSuwakoRightBarGui;
	// Suwako disabled bar
	*(const float **)0x7605A4 = &sanaeSuwakoLeftBarGui;
	*(const float **)0x7605AF = &sanaeSuwakoRightBarGui;
	SokuLib::TamperNearJmp(0x7605A8, fixupSanaeSuwakoBarDisabled);
	// Suwako disabled cross
	*(const float **)0x760667 = &sanaeSuwakoLeftBarGui;
	SokuLib::TamperNearJmp(0x760686, fixupSanaeSuwakoCrossDisabled);

	*(char *)0x4552C2 = 0x56;
	SokuLib::TamperNearCall(0x4552C3, generateClientDecks);
	*(char *)0x4552C8 = 0x5E;
	memset((void *)0x4552C9, 0x90, 0x4552E8 - 0x4552C9);

	new SokuLib::Trampoline(0x435377, onProfileChanged, 7);
	new SokuLib::Trampoline(0x450121, onDeckSaved, 6);

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
	//*(char *)0x48219D = 0x4;
	// Disable inputs for all 4 characters in transitions
	*(char *)0x479714 = 0x4;

	// Prevent rotation reset for Sanae's fly
	memset((void *)0x74B619, 0x90, 5);

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
	// Disable card highlight effect
	SokuLib::TamperNearJmp(0x47F973, 0x47FA4D);

	SokuLib::TamperNearCall(0x48227C, disableP34Pause);
	*(char *)0x482281 = 0x90;

	new SokuLib::Trampoline(0x4796EE, updateOtherHud, 5);

	SokuLib::TamperNearJmp(0x469C70, preventCardCost);

	// Extra key in key config
	*(char *)0x4512AC = 0x8;
	*(char *)0x4511EA = 0xC;
	*(char *)0x45135A = 0xC;
	*(char *)0x4513A7 = 0xC;
	*(char *)0x451650 = 0xC;
	*(char *)0x4514DC = 0xC;

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
	*(char *)0x47D6E5 = 0x90;
	*(char *)0x47D6E6 = 0x90;
	new SokuLib::Trampoline(0x47D18E, clearHealthRegen, 7);
	*(char *)0x47D193 = 0x90;
	*(char *)0x47D194 = 0x90;

	new SokuLib::Trampoline(0x4209F6, onChrSelectComplete, 6);
	new SokuLib::Trampoline(0x42151E, onStageSelectCancel_hook, 6);

	SokuLib::TamperNearCall(0x43890E, getOgHud);
	*(char *)0x438913 = 0x90;

	new SokuLib::Trampoline(0x46DEC1, saveOldHud, 5);
	new SokuLib::Trampoline(0x46DEC6, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF02, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF44, swapStuff, 5);
	new SokuLib::Trampoline(0x46DF80, swapStuff, 5);
	new SokuLib::Trampoline(0x46DFC0, swapStuff, 5);
	SokuLib::TamperNearJmp(0x46E002, restoreOldHud);

	*(char *)0x44CFEE = 0x50;
	SokuLib::TamperNearCall(0x44CFD8, selectProfileSpriteColor);

	// Filesystem first patch
	*(char *)0x40D1FB = 0xEB;
	*(char *)0x40D27A = 0x74;
	*(char *)0x40D27B = 0x91;
	*(char *)0x40D245 = 0x1C;
	memset((char *)0x40D27C, 0x90, 7);

	og_SelectConstruct = SokuLib::TamperNearJmpOpr(0x41E55F, CSelect_construct);
	og_SelectServerConstruct = SokuLib::TamperNearJmpOpr(0x41E644, CSelectSV_construct);
	og_SelectClientConstruct = SokuLib::TamperNearJmpOpr(0x41E6EF, CSelectCL_construct);

	SokuLib::TamperNearCall(0x42112C, renderChrSelectChrDataGear_hook);
	*(char *)0x421131 = 0x90;
	SokuLib::TamperNearCall(0x420DAD, renderChrSelectChrData_hook);
	*(char *)0x420DB2 = 0x90;
	*(char *)0x420DB3 = 0x90;
	SokuLib::TamperNearCall(0x4212D9, renderChrSelectChrName_hook);
	*(char *)0x4212DE = 0x90;
	SokuLib::TamperNearCall(0x421350, renderChrSelectProfile_hook);
	*(char *)0x421355 = 0x90;

	memset((void *)0x4208C4, 0x90, 8);
	SokuLib::TamperNearCall(0x4208C4, firstChrSelected);
	*(char *)0x4208F3 = 0x07;
	*(char *)0x4208FC = 0x07;
	*(char *)0x422865 = 0x07;
	*(char *)0x4227BD = 0x07;
	SokuLib::TamperNearJmpOpr(0x420683, selectRandomDeck_hook);
	SokuLib::TamperNearJmpOpr(0x42079B, selectRandomDeck_hook);
	memset((void *)0x420669, 0x90, 0x12);
	memset((void *)0x42078C, 0x90, 0xF);
	*(void **)0x42094C = chrSelectLastStep_hook;
	*(char *)0x420751 = 5;
	SokuLib::TamperNearJmpOpr(0x4204F6, chrSelectExtraSteps_hook);

	memset((void *)0x42081E, 0x90, 0x42085F - 0x42081E);
	SokuLib::TamperNearCall(0x420849, changePalette_hook0);
	memset((void *)0x4206DC, 0x90, 0x42072D - 0x4206DC);
	SokuLib::TamperNearCall(0x4206DC, changePalette_hook1);

	memset((void *)0x46012A, 0x90, 13);
	memset((void *)0x4622A4, 0x90, 13);

	const unsigned char profileExtraInit[] = {
		// The top part here is exactly what the game did before but shorter in size to fit more assembly
		// Before the game did:
		// mov [esi+00000188],0x00000000
		// mov [esi+0000018C],0x00000001
		// ...
		0x31, 0xC0,                                                // xor eax,eax
		0x89, 0x86, 0x88, 0x01, 0x00, 0x00,                        // mov [esi+00000188],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0x8C, 0x01, 0x00, 0x00,                        // mov [esi+0000018C],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0x90, 0x01, 0x00, 0x00,                        // mov [esi+00000190],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0x94, 0x01, 0x00, 0x00,                        // mov [esi+00000194],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0x98, 0x01, 0x00, 0x00,                        // mov [esi+00000198],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0x9C, 0x01, 0x00, 0x00,                        // mov [esi+0000019C],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0xA0, 0x01, 0x00, 0x00,                        // mov [esi+000001A0],eax
		0x40,                                                      // inc eax
		0x89, 0x86, 0xA4, 0x01, 0x00, 0x00,                        // mov [esi+000001A4],eax
		0x90,                                                      // nop
		0x90,                                                      // nop
		0x90,                                                      // nop
		0xC7, 0x86, 0x70, 0x01, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00 // mov [esi+00000170],0000001C
	};
	memcpy((void*)0x435988, profileExtraInit, sizeof(profileExtraInit));

	if (*(unsigned char *)0x46C900 == 0xE9)
		grInputHook = SokuLib::TamperNearJmpOpr(0x46C900, 0);
	SokuLib::TamperNearJmp(0x46C902, onChrKeys);
	memset((void *)0x46C8F4, 0x90, 14);
	SokuLib::TamperNearJmp(0x46C8F4, controllerUpdateOverride);
	*(char *)0x46C907 = 0x90;

	// Enable twilight weather
	*(int *)0x483F3C = *(int *)0x483F38;
	*(void (**)())0x483F38 = weatherFct;
	*(char *)0x483DC4 = 0x14;

	// Enable select key in netplay
	*(unsigned short *)0x454CD8 = 0b101111111111;
	*(unsigned short *)0x454CC2 = 0b101111111111;
	*(unsigned short *)0x4559D7 = 0b101111111111;
	*(unsigned short *)0x42DA31 = 0b101111111111;
	*(unsigned short *)0x42DA17 = 0b101111111111;
	*(unsigned short *)0x42ADEE = 0b101111111111;
	*(unsigned short *)0x42AE91 = 0b101111111111;


	// On hit
	SokuLib::TamperNearJmpOpr(0x47B17C, turnAroundHitBlock_hook);
	// On rightblock
	SokuLib::TamperNearJmpOpr(0x47C2A3, turnAroundHitBlock_hook);
	// On wrongblock
	SokuLib::TamperNearJmpOpr(0x47C53C, turnAroundHitBlock_hook);


	SokuLib::TamperNearCall(0x42A554, getCharacterIndexResetHealth_hook);
	*(char *)0x42A559 = 0x90;
	*(char *)0x42A59D = 4;

	memset((void *)0x47B592, 0x90, 10);
	SokuLib::TamperNearCall(0x47B592, setGrazeFlags);

	// Proration bug fix
	memmove((void *)0x4799CD, (void *)0x4799D1, 0x4799F2 - 0x4799D1);
	*(unsigned *)0x4799EE = 0x0424448B;
	*(char *)0x47AF43 = 0x86; // fld dword ptr [eax+000004B0] -> fld dword ptr [esi+000004B0]

	// Make the top functions use EDI so ESI is kept as is
	// push esi -> push edi
	*(char *)0x464A81 = 0x57;
	// mov esi,ecx -> mov edi,ecx
	*(char *)0x464A83 = 0xF9;
	// mov eax,[esi+000001C0] -> mov eax,[edi+000001C0]
	*(char *)0x464A8A = 0x87;
	// pop esi -> pop edi
	*(char *)0x464A97 = 0x5F;

	// push esi -> push edi
	*(char *)0x464AB1 = 0x57;
	// mov esi,ecx -> mov edi,ecx
	*(char *)0x464AB3 = 0xF9;
	// mov eax,[esi+000001C0] -> mov eax,[edi+000001C0]
	*(char *)0x464ABA = 0x87;
	// pop esi -> pop edi
	*(char *)0x464AC7 = 0x5F;

	// We use ESI here which has been leaked from the top call. It is the object that's hitting us.
	// mov eax,[ecx+00000170] -> mov eax,[esi+0000016C]
	*(char *)0x463F5A = 0x86;
	*(char *)0x463F5B = 0x6C;
	// fmul dword ptr [esi+000004B0] -> fmul dword ptr [eax+000004B0]
	*(char *)0x463F83 = 0x88;

	SokuLib::TamperNearCall(0x46404C, takeOpponentCorrection);
	*(char *)0x464051 = 0x90;

	// fmul dword ptr [esi+000004B0] -> fmul dword ptr [edi+000004B0]
	*(char *)0x463DC5 = 0x8F;
	// mov al,[esi+000004AD] -> mov al,[edi+000004AD]
	*(char *)0x463E32 = 0x87;

	// mov ecx,[edi+0000016C] -> mov ecx,[esi+0000016C]
	*(char *)0x47AE90 = 0x8E;
	// mov eax,[edi+0000016C] -> mov eax,[esi+0000016C]
	*(char *)0x47B016 = 0x86;
	*(char *)0x47B057 = 0x86;
	*(char *)0x47B048 = 0x86;
	*(char *)0x47B004 = 0x86;
	*(char *)0x47AB70 = 0x86;
	*(char *)0x47AB9A = 0x86;
	// mov edx,[edi+0000016C] -> mov edx,[esi+0000016C]
	*(char *)0x47ABC6 = 0x96;
	*(char *)0x47AEB2 = 0x96;

	// mov eax,[esi+0000016C] -> mov eax,[edi+0000016C]
	*(char *)0x47B25E = 0x87;
	*(char *)0x47B276 = 0x87;
	*(char *)0x47B28A = 0x87;


	// We swap the left and right combo displays
	// Original code:
	//0047e2c0 8b 44 24 14     MOV        EAX,dword ptr [ESP + 0x14]=>local_10
	//0047e2c4 8b 4c 24 18     MOV        ECX,dword ptr [ESP + 0x18]=>local_c
	//0047e2c8 8d 04 b0        LEA        EAX,[EAX + ESI*0x4]
	//0047e2cb 03 05 e4        ADD        EAX,dword ptr [CBattleManagerPtr]
	//         85 89 00
	//0047e2d1 8b 44 04 1c     MOV        EAX,dword ptr [ESP + EAX*0x1 + 0x1c]
	//0047e2d1 8b 44 04 1c     MOV        EAX,dword ptr [ESP + EAX*0x1 + 0x1c]
	//0047e2d5 50              PUSH       EAX
	//0047e2d6 51              PUSH       ECX
	//0047e2d7 8b cf           MOV        ECX,EDI
	//0047e2d9 89 44 b4 24     MOV        dword ptr [ESP + ESI*0x4 + 0x24]=>local_4,EAX
	memset((void *)0x47E2C0, 0x90, 0x47E2DD - 0x47E2C0);
	SokuLib::TamperNearCall(0x47E2C4, swapComboCtrs);

	*(const char **)0x451986 = keyConfigDatPath;

	SokuLib::TamperNearCall(0x45409A, parseExtraChrsGameMatch_hook);
	*(char *)0x45409F = 0x90;
	SokuLib::TamperNearCall(0x453F75, addExtraChrsGameMatch_hook);

	new SokuLib::Trampoline(0x428789, generateFakeDecks, 5);
	new SokuLib::Trampoline(0x428521, generateFakeDecks, 5);

	SokuLib::TamperNearCall(0x487873, onMeterGained_hook);
	*(char *)0x487878 = 0x90;
	SokuLib::TamperNearCall(0x468B00, onMeterGainedHit_hook);
	*(char *)0x468B05 = 0x90;
	*(char *)0x468B06 = 0x90;

	og_loadDat = SokuLib::TamperNearJmpOpr(0x7fb85f, loadExtraDatFiles);

	og_handleInputs = SokuLib::TamperNearJmpOpr(0x48224D, handlePlayerInputs);
	s_origLoadDeckData = SokuLib::TamperNearJmpOpr(0x437D23, loadDeckData);
	og_getSaveDataMgr = SokuLib::TamperNearJmpOpr(0x4818FA, onLoadingDone);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	::FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
	return TRUE;
}

// New mod loader functions
// Loading priority. Mods are loaded in order by ascending level of priority (the highest first).
// When 2 mods define the same loading priority the loading order is undefined.
extern "C" __declspec(dllexport) int getPriority()
{
	return 50;
}