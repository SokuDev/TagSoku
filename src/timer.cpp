//
// Created by PinkySmile on 03/08/25.
//

#include <cmath>
#include <SokuLib.hpp>
#include "timer.hpp"

#ifdef _DEBUG
#define printf(...)
#define puts(...)
#endif

#define EXPECTED_DURATION 180
#define CRUSH_INTERVAL 5

const unsigned multiplier = std::ceil(EXPECTED_DURATION * 60 / 99);

TimerState timerState;
static SokuLib::DrawUtils::Sprite digits;
static SokuLib::SWRFont font;
static bool init = false;
static unsigned short displayed;

int renderTimer(SokuLib::BattleManager *This)
{
	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_PRACTICE)
		return 0;
	if (This->matchState < 6 && This->matchState >= 1 && !timerState.ended) {
		if (timerState.crushing)
			digits.tint = SokuLib::Color::Red;
		else
			digits.tint = SokuLib::Color::White;
		if (displayed >= 10) {
			digits.setPosition({299, 440});
			digits.rect.left = displayed / 10 * 11;
			digits.draw();
			digits.setPosition({322, 440});
			digits.rect.left = displayed % 10 * 11;
			digits.draw();
		} else {
			digits.setPosition({309, 440});
			digits.rect.left = displayed * 11;
			digits.draw();
		}
	}
	return 0;
}

void punish(unsigned char &orb)
{
	if (orb == 0) {
		if (timerState.ended)
			return;
		timerState.ended = true;
		puts("Activate Typhoon");
		return SokuLib::activateWeather(SokuLib::WEATHER_TYPHOON, 1);
	}
	orb--;
	puts("Crush");
	SokuLib::playSEWaveBuffer(38);
}

static void playBGM(const char *bgm, bool expectedState)
{
	printf("Playing %s\n", bgm);
	SokuLib::playBGM(bgm);
	puts("Done");
}

void updateTimer()
{
	SokuLib::BattleManager *This = &SokuLib::getBattleMgr();

	if (!init) {
		digits.texture.loadFromGame("data/battle/weatherFont001.cv0");
		digits.setSize({22, 36});
		digits.rect.width = 11;
		digits.rect.height = 18;
		init = true;
	}

	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_PRACTICE)
		return;

	// Second call is to check if the game is paused
	if (This->matchState < 1 || ((bool (*)())0x0043e740)())
		return;

	if (This->matchState != 2) {
		if (timerState.ended)
			SokuLib::weatherCounter = 0;
		if (timerState.crushing && This->currentRound != 0) {
			auto file = "data/bgm/st" + std::string(SokuLib::gameParams.musicId < 10 ? "0" : "") + std::to_string(SokuLib::gameParams.musicId) + ".ogg";

			playBGM(file.c_str(), false);
		}
		timerState.timer = 99 * multiplier;
		timerState.orbsLeft = 5;
		timerState.orbsRight = 5;
		timerState.ended = false;
		timerState.crushing = false;
	} else if (!timerState.ended) {
		timerState.timer--;
		if (timerState.timer == 0) {
			if (!timerState.crushing)
				playBGM("data/bgm/st36.ogg", true);
			timerState.crushing = true;
			timerState.timer = CRUSH_INTERVAL * 60;

			auto lPool = SokuLib::v2::GameDataManager::instance->players[0]->hp + SokuLib::v2::GameDataManager::instance->players[2]->hp;
			auto rPool = SokuLib::v2::GameDataManager::instance->players[1]->hp + SokuLib::v2::GameDataManager::instance->players[3]->hp;

			if (lPool < rPool)
				punish(timerState.orbsLeft);
			else if (lPool > rPool)
				punish(timerState.orbsRight);
			else {
				punish(timerState.orbsLeft);
				punish(timerState.orbsRight);
			}
		}
	}
	if (This->matchState <= 2) {
		if (!timerState.ended) {
			if (timerState.crushing) {
				if (timerState.timer % 60 == 0 && timerState.timer != CRUSH_INTERVAL * 60) {
					puts("Tick");
					SokuLib::playSEWaveBuffer(59);
				}
				displayed = (timerState.timer + 59) / 60;
			} else if (timerState.timer % multiplier == 0)
				displayed = (timerState.timer + multiplier - 1) / multiplier;
		}
		if (timerState.ended)
			SokuLib::weatherCounter = 999;
		if (timerState.crushing) {
			if (This->leftCharacterManager.maxSpirit >= timerState.orbsLeft * 200) {
				This->leftCharacterManager.maxSpirit = timerState.orbsLeft * 200;
				This->leftCharacterManager.timeWithBrokenOrb = 0;
			}
			if (This->rightCharacterManager.maxSpirit >= timerState.orbsRight * 200) {
				This->rightCharacterManager.maxSpirit = timerState.orbsRight * 200;
				This->rightCharacterManager.timeWithBrokenOrb = 0;
			}
		}
	}
}