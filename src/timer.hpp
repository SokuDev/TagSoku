//
// Created by PinkySmile on 03/08/25.
//

#ifndef TAGSOKU_TIMER_HPP
#define TAGSOKU_TIMER_HPP


#include <SokuLib.hpp>

struct TimerState {
	unsigned short timer;
	unsigned char orbsLeft;
	unsigned char orbsRight;
	bool crushing;
	bool ended;
};

void updateTimer();
int renderTimer(SokuLib::BattleManager *This);

extern TimerState timerState;


#endif //TAGSOKU_TIMER_HPP
