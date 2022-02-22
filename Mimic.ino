// requires compiler flags:
//-fno-inline
// uses Custom Blinklib

#include <blinklib.h>

//#include "Serial.h"

#define BROWN dim(ORANGE, 100)

enum signalStates {
  SETUP,             // 0 , 0
  GAME,              // 1 , 4
  GAME_EVEN,         // 1 , 4
  GAMEOVER_LOSE,     // 2, 8
  GAMEOVER_WIN,      // 3, 12
  BEATDROP,          // 4, 16
  GAMEOVER_NOTSENT,  // 5, 20 this shouldn't be sent!
  INERT,             // 6, 24
  INERT_BRANCH,      // 7, 28
  INERT_CONTROLLER,  // 8, 32
  RESOLVE,           // 9, 36
  RESOLVE_BRANCH,    // 10, 40
  RESOLVE_CONTROLLER // 11, 44
};
byte signalState = INERT;
byte gameMode = SETUP;

bool evenSent = true;
bool amEven = false;

enum lifeLossStates { LL_INERT, LL_LOSE_LIFE, LL_REVERT };
byte lifeLostState[6] = {LL_INERT, LL_INERT, LL_INERT,
                         LL_INERT, LL_INERT, LL_INERT};
byte lifeLossCounter = 0;

enum games { WAIT, FAIL, ROTATE_CW, ROTATE_ACW, FLIP, DOUBLE_C, TRIPLE_C };
byte game = WAIT;

enum blinkModes { UNUSED, GAMEPLAYER, CONNECTOR, BRANCH, CONTROLLER };
byte blinkMode = UNUSED;

byte originalFace = 7;
byte connectedFace = 0;
byte targetFace = 7;
bool connectedFaceSet = false;
byte score = 0;

byte gameOverFace = 7;
bool gameOver = false;
bool gameOverPublished = false;

#define PULSE_LENGTH 750

// ServicePortSerial sp;

Timer gameTimer;
Timer startTimer;
Timer missingPlayerTimer;

#define MAX_TIME_DISCONECTED 5000

////---Solo 6 Blinks Intensity---
//#define GAMELENGTH 10000  //sets the time to complete the task
//#define startMultiplier 2000 //mutplier for the time between tasks (against a
// random 1-5) #define startRandomiserSize 9 #define BeatDropLength 3 //number
// of beatdrop itterations #define BeatDropWaitTimeModifier 3000 //multiply by
// 15-20

//
////---Low Intensity---
//#define GAMELENGTH 5000  //sets the time to complete the task
//#define startMultiplier 1000 //mutplier for the time between tasks (against a
//// random 1-5)
//#define startRandomiserSize 4
//#define BeatDropLength 3 //number of beatdrop itterations
//#define BeatDropWaitTimeModifier 1000 //multiply by 15-20

//---Medium Intensity---
#define GAMELENGTH 2000
#define startMultiplier 750
#define startRandomiserSize 4
#define BeatDropLength 4
#define BeatDropWaitTimeModifier 1000 // multiply by 15-20

byte level = 0;
#define MAX_LEVEL 20

//---High Intensity---
//#define GAMELENGTH 2000
//#define startMultiplier 500
//#define startRandomiserSize 4
//#define BeatDropLength 4
//#define BeatDropWaitTimeModifier 750 //multiply by 15-20

#define BeatDropWarnTime 4000
#define BeatDropResetTime 3000

#define failLength 1000

#define slowControllerPulseTime 750
#define fastControllerPulseTime 333
byte countBeatDrop = 0;
bool startBeatDrop = false;

byte countNeighbours = 0;
byte connections = 0;

static const Color colorWheel[6] = {MAGENTA, YELLOW, CYAN, RED, BLUE, GREEN};
byte wheelCycle = 0;
bool wheelCycleFlag = false;
bool flipFace[6] = {false, false, false, false, false, false};
byte faceCycle[6] = {0, 0, 0, 0, 0, 0};

void setup() {
  randomize();
  setColor(OFF);
  setValueSentOnAllFaces(INERT << 2);
  gameTimer.never();
  startTimer.never();
  missingPlayerTimer.never();
  // sp.begin();
}

void loop() {
  countNeighbours = 0;
  bool firstPrint = true;
  byte firstConnectedNeighbour;
  FOREACH_FACE(f) {
    //  for(byte x = 6; x >= 1 ; --x) {
    //    byte f = x-1;
    if (!isValueReceivedOnFaceExpired(f)) {
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      firstConnectedNeighbour = f;
      countNeighbours++;

      if (CONTROLLER != blinkMode &&
          (INERT_CONTROLLER == val || RESOLVE_CONTROLLER == val)) {
        changeConnectedFace(f);
      }

      // if (INERT != val && INERT_BRANCH != val && INERT_CONTROLLER != val) {
      //   if (firstPrint) {
      //     sp.println(signalState);
      //     sp.println(gameMode);
      //     firstPrint = false;
      //   }
      //   sp.print(f);
      //   sp.print(" ");
      //   sp.println(val);
      // }
    }
  }

  if ((CONNECTOR == blinkMode || BRANCH == blinkMode) &&
      connections > countNeighbours) {
    if (missingPlayerTimer.isExpired()) {
      // been disconnected for too long, lose a life
      receivedLLLoseLife(7);
      missingPlayerTimer.set(MAX_TIME_DISCONECTED);
    } else if (missingPlayerTimer.getRemaining() > MAX_TIME_DISCONECTED) {
      missingPlayerTimer.set(MAX_TIME_DISCONECTED);
    }
  } else {
    missingPlayerTimer.never();
  }

  if (1 == countNeighbours && isValueReceivedOnFaceExpired(connectedFace) &&
      GAMEPLAYER == blinkMode) {
    if (connectedFace != firstConnectedNeighbour) {
      changeConnectedFace(firstConnectedNeighbour);
    }
  }

  if (CONTROLLER == blinkMode && INERT == signalState && gameOver &&
      !gameOverPublished) {
    signalState = GAMEOVER_NOTSENT;
    gameMode = GAMEOVER_NOTSENT;
    gameOverPublished = true;
  }

  if (signalState < INERT) {
    sendLoop();
  } else if (INERT == signalState) {
    inertLoop();
  } else if (RESOLVE == signalState) {
    resolveLoop();
  }

  FOREACH_FACE(f) {
    byte llval = lifeLostState[f];
    if (!isValueReceivedOnFaceExpired(f)) {
      byte payload = getPayload(getLastValueReceivedOnFace(f));
      switch (llval) {
      case LL_INERT:
        if (LL_LOSE_LIFE == payload) {
          receivedLLLoseLife(f);
        } else if (LL_INERT == payload && connectedFace == f &&
                   lifeLossCounter > 0) {
          lifeLostState[f] = LL_LOSE_LIFE;
          --lifeLossCounter;
        }
        break;
      case LL_LOSE_LIFE:
        if (LL_INERT != payload) {
          lifeLostState[f] = LL_REVERT;
        }
        break;
      case LL_REVERT:
        if (LL_LOSE_LIFE != payload) {
          lifeLostState[f] = LL_INERT;
        }
        break;
      }
    }
  }
  long pulseProgress = millis() % PULSE_LENGTH;
  byte pulseMapped = (pulseProgress * 255) / PULSE_LENGTH;

  if (SETUP == gameMode) {
    setupLoop();
    //    drawSetup();
    setColor(OFF);
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
        setColorOnFace(BLUE, f);
      }
    }
    if (1 == countNeighbours) {
      setColor(GREEN);
    }
    if (CONTROLLER == blinkMode) {
      setColor(MAGENTA);
    } else if (BRANCH == blinkMode) {
      setColor(BROWN);
    }
  } else if (GAME == gameMode) {

    gameLoop();
    if (GAMEPLAYER == blinkMode) {
      int mils = millis() % 1000;
      setColor(OFF);
      switch (game) {
      case WAIT:
        setColor(GREEN);
        break;
      case FAIL:
        setColor(RED);
        break;
      case ROTATE_CW:
        drawRotate(true, pulseMapped);
        break;
      case ROTATE_ACW:
        drawRotate(false, pulseMapped);
        break;
      case FLIP:
        drawFlip();
        break;
      //          case SINGLE_C:
      //            drawSingleClick();
      //            break;
      case DOUBLE_C:
        // drawDoubleClick();
        if (mils > 750) {
          //    setColor(BLUE);
          setColorOnFace(BLUE, 0);
          setColorOnFace(BLUE, 1);
          setColorOnFace(BLUE, 2);
        } else if (mils > 400 && mils <= 650) {
          //    setColor(BLUE);
          setColorOnFace(BLUE, 3);
          setColorOnFace(BLUE, 4);
          setColorOnFace(BLUE, 5);
        }
        break;
      case TRIPLE_C:
        // drawTripleClick();

        mils = millis() % 1350;
        if (mils > 1100) {
          //    setColor(BLUE);
          setColorOnFace(BLUE, 0);
          setColorOnFace(BLUE, 1);
        } else if (mils > 400 && mils <= 650) {
          //    setColor(BLUE);
          setColorOnFace(BLUE, 2);
          setColorOnFace(BLUE, 3);
        } else if (mils > 750 && mils <= 1000) {
          //    setColor(BLUE);
          setColorOnFace(BLUE, 4);
          setColorOnFace(BLUE, 5);
        }
        break;
      }
    } else if (CONTROLLER == blinkMode) {
      drawControllerPulse();
    } else {
      drawScore(BRANCH == blinkMode, pulseMapped);
    }
  } else if (GAMEOVER_NOTSENT == gameMode || GAMEOVER_LOSE == gameMode ||
             GAMEOVER_WIN == gameMode) {
    if (CONTROLLER == blinkMode) {
      if (buttonDoubleClicked()) {
        receivedSetup();
      }
      //    }
      // drawGameOver(); - inlined
      //    if (CONTROLLER == blinkMode) {
      setColor(GREEN);
      setColorOnFace(RED, gameOverFace);
    } else {
      if (GAMEOVER_WIN == gameMode) {
        setColor(GREEN);
      } else {
        setColor(RED);
      }
    }
  }

  // dump button data
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonPressed();
  buttonMultiClicked();

  FOREACH_FACE(f) {
    byte sendData = signalState;
    if (CONTROLLER == blinkMode && GAMEOVER_NOTSENT == signalState) {
      if (gameOverFace == f) {
        sendData = GAMEOVER_LOSE;
      } else {
        sendData = GAMEOVER_WIN;
      }
    }
    if (INERT == signalState) {
      if (CONTROLLER == blinkMode) {
        sendData = INERT_CONTROLLER;
      }
      if ((BRANCH == blinkMode || CONNECTOR == blinkMode ||
           UNUSED == blinkMode) &&
          connectedFaceSet) {
        // publish path back to controller to opposite faces
        if ((connectedFace + 3) % 6 == f || (connectedFace + 2) % 6 == f ||
            (connectedFace + 4) % 6 == f) {
          sendData = INERT_CONTROLLER;
        } else {
          sendData = INERT_BRANCH;
        }
      }
    } else if (RESOLVE == signalState) {
      if (CONTROLLER == blinkMode) {
        sendData = RESOLVE_CONTROLLER;
      }
      if (BRANCH == blinkMode || CONNECTOR == blinkMode) {
        // publish path back to controller to opposite faces
        if ((connectedFace + 3) % 6 == f || (connectedFace + 2) % 6 == f ||
            (connectedFace + 4) % 6 == f) {
          sendData = RESOLVE_CONTROLLER;
        } else {
          sendData = RESOLVE_BRANCH;
        }
      }
    } else if (GAME == signalState) {
      if (BRANCH == blinkMode || CONNECTOR == blinkMode) {
        if (!isValueReceivedOnFaceExpired(f) &&
            ((connectedFace + 3) % 6 == f || (connectedFace + 2) % 6 == f ||
             (connectedFace + 4) % 6 == f)) {
          if (!evenSent) {
            sendData = GAME_EVEN;
            evenSent = true;
          } else {
            evenSent = false;
          }
        }
      }
    }
    sendData = sendData << 2;

    sendData = sendData + lifeLostState[f];

    setValueSentOnFace(sendData, f);
  }

  // if (signalState < INERT) {
  //   setColorOnFace(CYAN, connectedFace);
  // } else if (signalState < RESOLVE) {
  //   setColorOnFace(ORANGE, connectedFace);
  // } else {
  //   setColorOnFace(YELLOW, connectedFace);
  // }
  // if(amEven) {
  //   setColorOnFace(MAGENTA, (connectedFace+1)%6);
  // }
}

void inertLoop() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      byte payload = getPayload(f);
      if (INERT > val) {
        switch (val) {
        case SETUP:
          if (CONTROLLER == blinkMode) {
            blinkMode = UNUSED;
          }
          receivedSetup();
          break;
        case GAME:
          receivedGame(f, false);
          break;
        case GAME_EVEN:
          receivedGame(f, true);
          break;
        case GAMEOVER_LOSE:
          receivedGameOver(f, true);
          break;
        case GAMEOVER_WIN:
          receivedGameOver(f, false);
          break;
        case BEATDROP:
          // manually inlined...
          signalState = BEATDROP;
          startBeatDrop = true;
          if (CONNECTOR == blinkMode || BRANCH == blinkMode) {
            gameTimer.set(GAMELENGTH * 4);
            FOREACH_FACE(face) {
              flipFace[face] = false;
              faceCycle[face] = 0;
            }
          }
          break;
        default:
          break;
        }
      }
    }
  }
}

void sendLoop() {
  byte countReceivers = 0;
  bool allSend = true;
  // look for neighbors who have not heard the GO news
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
      countReceivers++;
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      if (INERT == val || INERT_BRANCH == val || INERT_CONTROLLER == val) {
        allSend = false;
      } else if (val < INERT && val != signalState) {
        // there's a clash
        // as we have multiple versions of the same state (GAMEOVER LOSE vs
        // WIN) we don't care about clashes especially as all states start at
        // the controller allSend = false;
      }
    }
  }
  if (allSend && countReceivers >= connections) {
    signalState = RESOLVE;
  }
}

void resolveLoop() {
  bool allResolve = true;
  bool anyResolve = false;
  byte countReceivers = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
      countReceivers++;

      byte val = getSignalState(getLastValueReceivedOnFace(f));

      if (val < INERT) { // This neighbor isn't in RESOLVE. Stay in RESOLVE
        allResolve = false;
      }
      // If controller, check if any neighbours are in resolve
      // else check if any neighbours away from the controller are in resolve
      if (RESOLVE <= val &&
          (CONTROLLER == blinkMode ||
           (connectedFace != f && ((connectedFace + 1) % 6 != f) &&
            ((connectedFace + 5) % 6 != f)))) {
        anyResolve = true;
      }
    }
  }

  if (allResolve && countReceivers >= connections) {
    // prevent controller moving to Inert until all neighbours are Inert
    // prevent branches from moving to Inert until all non-connectedFace
    // blinks are Intert
    if (!anyResolve) {
      signalState = INERT;
    }
  }
}

// void LLInertLoop(byte face) {
//   if (!isValueReceivedOnFaceExpired(face)) {
//     byte payload = getPayload(getLastValueReceivedOnFace(face));
//     if (LL_LOSE_LIFE == payload) {
//       receivedLLLoseLife(face);
//     } else if (LL_INERT == payload && connectedFace == face &&
//                lifeLossCounter > 0) {
//       lifeLostState[face] = LL_LOSE_LIFE;
//       --lifeLossCounter;
//     }
//   }
// }

// void LLSendLoop(byte face) {
//   if (!isValueReceivedOnFaceExpired(face)) {
//     byte payload = getPayload(getLastValueReceivedOnFace(face));
//     if (LL_INERT != payload) {
//       lifeLostState[face] = LL_REVERT;
//     }
//   }
// }

// void LLResolveLoop(byte face) {
//   if (!isValueReceivedOnFaceExpired(face)) {
//     byte payload = getPayload(getLastValueReceivedOnFace(face));
//     if (LL_LOSE_LIFE != payload) {
//       lifeLostState[face] = LL_INERT;
//     }
//   }
// }

void receivedLLLoseLife(byte face) {
  switch (blinkMode) {
  case CONTROLLER:
    // game over
    if (!gameOver) {
      gameOverFace = face;
      gameOver = true;
      startTimer.never();
      gameTimer.never();
    }
    lifeLostState[face] = LL_REVERT;
    break;
  case GAMEPLAYER:
    // ignore?
    break;
  case CONNECTOR:
    if (6 > score) {
      score++;
    } else {
      if (LL_INERT == lifeLostState[connectedFace] &&
          !isValueReceivedOnFaceExpired(connectedFace) &&
          LL_INERT == getPayload(getLastValueReceivedOnFace(connectedFace))) {
        lifeLostState[connectedFace] = LL_LOSE_LIFE;
      } else {
        lifeLossCounter++;
      }
    }
    if (face != 7) {
      lifeLostState[face] = LL_REVERT;
    }
    break;
  case BRANCH:
    if (LL_INERT == lifeLostState[connectedFace] &&
        !isValueReceivedOnFaceExpired(connectedFace) &&
        LL_INERT == getPayload(getLastValueReceivedOnFace(connectedFace))) {
      lifeLostState[connectedFace] = LL_LOSE_LIFE;
    } else {
      lifeLossCounter++;
    }
    if (face != 7) {
      lifeLostState[face] = LL_REVERT;
    }
    break;
  }
}

void receivedSetup() {
  gameOver = false;
  gameOverPublished = false;
  countBeatDrop = 0;
  startBeatDrop = false;
  score = 0;
  signalState = SETUP;
  gameMode = SETUP;
  // connectedFace = 7;
  targetFace = 7;
  gameTimer.never();
  startTimer.never();
  connections = 0;
  FOREACH_FACE(f) {
    lifeLostState[f] = LL_INERT;
    flipFace[f] = false;
    faceCycle[f] = 0;
  }
  lifeLossCounter = 0;
  if (BRANCH > blinkMode) {
    blinkMode = UNUSED;
  }
  level = 0;
  evenSent = true;
  amEven = false;
  connectedFaceSet = false;
  wheelCycle = 0;
  wheelCycleFlag = false;
}

void receivedGame(byte face, bool even) {
  connections = countNeighbours;
  signalState = GAME;
  gameMode = GAME;

  amEven = even;
  // connectedFace = face;
  if (1 == countNeighbours) {
    blinkMode = GAMEPLAYER;
    startTimer.set(getLevelStartTime());
    game = WAIT;
    //              sp.println("game is 0");
    gameTimer.never();
  } else if (BRANCH != blinkMode) {
    blinkMode = CONNECTOR;
  } else { // mode is Branch
    score = 6;
  }
}

void receivedGameOver(byte face, bool lost) {
  // bit of a tricky one - dont want to take game over from wrong lane
  // try and make sure we only listen to connected face on the branches

  if (lost && (face == connectedFace || GAMEPLAYER == blinkMode)) {
    signalState = GAMEOVER_LOSE;
    gameMode = GAMEOVER_LOSE;
  } else {
    signalState = GAMEOVER_WIN;
    gameMode = GAMEOVER_WIN;
  }
  gameTimer.never();
  startTimer.never();
}

// void receivedBeatDrop(byte face) {
//   signalState = BEATDROP;
//   startBeatDrop = true;
//   if (CONNECTOR == blinkMode || BRANCH == blinkMode) {
//     gameTimer.set(GAMELENGTH * 4);
//   }
// }

void setupLoop() {
  if (CONTROLLER == blinkMode) {
    if (buttonDoubleClicked() && INERT == signalState) {
      signalState = GAME;
      gameMode = GAME;
      startTimer.set(4000);
      startBeatDrop = false;
    } else if (buttonSingleClicked()) {
      blinkMode = UNUSED;
    }
  } else {
    if (buttonDoubleClicked()) {
      blinkMode = CONTROLLER;
    } else if (buttonSingleClicked()) {
      if (BRANCH == blinkMode) {
        blinkMode = UNUSED;
      } else if (UNUSED == blinkMode) {
        blinkMode = BRANCH;
      }
    }
  }
}

void setupForNextGame() {
  if (countBeatDrop > 0) {
    countBeatDrop++;
  }
  if (countBeatDrop > 0 && getBeatDropLength() >= countBeatDrop) {
    startTimer.set(100);
  } else {
    countBeatDrop = 0;
    startTimer.set(getLevelStartTime());
    if (MAX_LEVEL > level) {
      level++;
    }
  }
  game = WAIT;
  gameTimer.never();
}

int getLevelStartTime() {
  if (0 == level) {
    return startMultiplier * (6 + (4 * amEven));
  }
  return startMultiplier * (6 - (level / 4));
  //  return startMultiplier * (random(startRandomiserSize - (level/4)) + 1);
  /*  switch (level / 4) {
      case(0):
        return startMultiplier * (random(2) + 3); // odd or even would be
    better case(1): return startMultiplier * (random(startRandomiserSize) +
    3); break; case(2): return startMultiplier * (random(startRandomiserSize)
    + 1); break; case(3): return startMultiplier * 2; break; case(4): return
    startMultiplier; break;
    }
    */
}

byte getBeatDropLength() {
  if (level < MAX_LEVEL) {
    return BeatDropLength;
  }
  return BeatDropLength + 2;
}

void failGame() {
  //  countBeatDrop = 0;
  game = FAIL;

  gameTimer.set(failLength);
  if (LL_INERT == lifeLostState[connectedFace]) {
    lifeLostState[connectedFace] = LL_LOSE_LIFE;
  } else {
    lifeLossCounter++;
  }
}

void gameLoop() {
  if (GAMEPLAYER == blinkMode) {
    //    byte currentFace = connectedFace;
    // FOREACH_FACE(f) {
    //   if (!isValueReceivedOnFaceExpired(f)) {
    //     currentFace = f; //this should be connectedFace
    //   }
    // }
    if (startTimer.isExpired() || (startBeatDrop && WAIT == game)) {
      if (startBeatDrop) {
        countBeatDrop++;
        startBeatDrop = false;
      }
      gameTimer.set(GAMELENGTH + (250 * (6 - (level / 4))));
      game = random(4) + 2;

      startTimer.never();
      originalFace = connectedFace;

      switch (game) {
      case ROTATE_CW:
        targetFace = (connectedFace + 5) % 6;
        break;
      case ROTATE_ACW:
        targetFace = (connectedFace + 1) % 6;
        break;
      case FLIP:
        targetFace = (connectedFace + 3) % 6;
        break;
      default:
        targetFace = connectedFace;
      }
      return;
    }
    if (gameTimer.isExpired()) {
      if (FAIL != game) {
        failGame();
        return;
      } else {
        if (LL_LOSE_LIFE ==
            lifeLostState[connectedFace]) { // lifeloss hasn't been received
          gameTimer.set(failLength);
          return;
        }
      }
      setupForNextGame();
      return;
    }
    if (!gameTimer.isExpired() && FAIL != game && WAIT != game) {
      byte clicks = 0;
      if (buttonMultiClicked()) {
        clicks = buttonClickCount();
      }
      bool sglClick = buttonSingleClicked();
      bool dblClick = buttonDoubleClicked();

      if (connectedFace != originalFace && connectedFace != targetFace &&
          connectedFace != 7) {
        failGame();

        return;
      } else if (game <= 4 && (clicks > 0 || dblClick || sglClick)) {
        failGame();

        return;
      }
      switch (game) {
      case ROTATE_CW:
        if (!isValueReceivedOnFaceExpired(targetFace)) {
          setupForNextGame();
        }
        break;
      case ROTATE_ACW:
        if (!isValueReceivedOnFaceExpired(targetFace)) {
          setupForNextGame();
        }
        break;
      case FLIP:
        if (!isValueReceivedOnFaceExpired(targetFace)) {
          setupForNextGame();
        }
        break;
      case DOUBLE_C:
        if (clicks > 2 || sglClick) {
          failGame();

          return;
        }
        if (dblClick) {
          setupForNextGame();
        }
        break;
      case TRIPLE_C:
        if (dblClick || sglClick) {
          failGame();

          return;
        }
        if (3 == clicks) {
          setupForNextGame();
        }
      }
    }
  } else {
    if (CONTROLLER == blinkMode) {
      if (startTimer.isExpired()) {
        // start beatdrop timer
        gameTimer.set(BeatDropWaitTimeModifier * (random(5) + 15));
        startTimer.never();
      } else if (gameTimer.isExpired() && !startBeatDrop) {
        // start flashing
        startBeatDrop = true;
        gameTimer.set(BeatDropWarnTime);
      } else if (gameTimer.isExpired() & startBeatDrop &&
                 INERT == signalState) {
        // send beatdrop instruction, stop flashing, wait until beatdrop might
        // be finished
        signalState = BEATDROP;
        startBeatDrop = false;
        wheelCycle = 0;
        wheelCycleFlag = false;
        gameTimer.never();
        startTimer.set(BeatDropResetTime);
      }
    }

    if (buttonDoubleClicked()) {
      receivedSetup();
    }
  }
}

// void gameOverLoop() {
//   if (buttonDoubleClicked()) {
//     receivedSetup();
//   }
// }

byte getSignalState(byte data) { return ((data >> 2)); }

byte getPayload(byte data) { return (data & 3); }

void changeConnectedFace(byte newFace) {
  if (connectedFace != newFace) {
    lifeLostState[newFace] = lifeLostState[connectedFace];
    lifeLostState[connectedFace] = LL_INERT;
    connectedFace = newFace;
  }
  connectedFaceSet = true;
}

void drawRotate(bool clockwise, byte pulseMapped) {

  byte dimness = 0;

  // set color
  FOREACH_FACE(f) {
    if (clockwise) {
      dimness = sin8_C(pulseMapped - (42 * f));
    } else {
      dimness = sin8_C(pulseMapped + (42 * f));
    }
    if(dimness<32) {
      dimness = 0;
    }
    setColorOnFace(dim(MAGENTA, dimness), f);
  }
}

void drawFlip() {

  // get progress from 0 - MAX
  long pulseProgress = millis() % PULSE_LENGTH;

  // transform that progress to a byte (64-192)
  byte pulseMapped = ((pulseProgress * 128) / PULSE_LENGTH) + 64;

  // transform that byte with sin
  byte dimness = sin8_C(pulseMapped);
  byte oppDimness = sin8_C((pulseMapped + 128) % 256);

  // set color
  FOREACH_FACE(f) {
    if (f == ((targetFace + 3) % 6)) {
      setColorOnFace(dim(YELLOW, dimness), f);
    } else if (f == targetFace) {
      setColorOnFace(dim(YELLOW, oppDimness), f);
    } else {
      setColorOnFace(OFF, f);
    }
  }
}

// void drawSingleClick() {
//   setColor(OFF);
//   if (millis() % 650 > 400) {
//     setColor(BLUE);
//   }
// }

// void drawDoubleClick() {
//   setColor(OFF);
//   int mils = millis() % 1000;
//   if (mils > 750) {
//     //    setColor(BLUE);
//     setColorOnFace(BLUE, 0);
//     setColorOnFace(BLUE, 1);
//     setColorOnFace(BLUE, 2);
//   } else if (mils > 400 && mils <= 650) {
//     //    setColor(BLUE);
//     setColorOnFace(BLUE, 3);
//     setColorOnFace(BLUE, 4);
//     setColorOnFace(BLUE, 5);
//   }
// }

// void drawTripleClick() {
//   setColor(OFF);
//   int mils = millis() % 1350;
//   if (mils > 1100) {
//     //    setColor(BLUE);
//     setColorOnFace(BLUE, 0);
//     setColorOnFace(BLUE, 1);
//   } else if (mils > 400 && mils <= 650) {
//     //    setColor(BLUE);
//     setColorOnFace(BLUE, 2);
//     setColorOnFace(BLUE, 3);
//   } else if (mils > 750 && mils <= 1000) {
//     //    setColor(BLUE);
//     setColorOnFace(BLUE, 4);
//     setColorOnFace(BLUE, 5);
//   }
// }

void drawScore(bool isBranch, byte pulseMapped) {

  if (startBeatDrop) {
    if (!gameTimer.isExpired()) {

      byte dimness = 0;

      int rotation = 1;

      // set color
      FOREACH_FACE(f) {
        if (f < 3) {
          rotation = -1;
        } else {
          rotation = 1;
        }
        dimness = sin8_C(pulseMapped + (42 * f * rotation));
        if (dimness < 32) {
          dimness = 0;
          if (flipFace[f]) {
            flipFace[f] = false;
            faceCycle[f] = (faceCycle[f] + 1) % 6;
          }
        } else if (dimness > 200) {
          flipFace[f] = true;
        }

        setColorOnFace(dim(colorWheel[faceCycle[f]], dimness),
                       ((f + connectedFace) % 6));
      }
    } else {
      startBeatDrop = false;
      gameTimer.never();
    }
  } else {
    if (!isBranch) {
      setColor(OFF);
      for (byte i = score; i > 0; --i) {
        setColorOnFace(RED, i - 1);
      }
    } else {
      setColor(BROWN);
    }
  }
}

// void drawGameOver() {
//   if (CONTROLLER == blinkMode) {
//     setColor(OFF);
//     setColorOnFace(RED, gameOverFace);
//   } else {
//     if (GAMEOVER_WIN == gameMode) {
//       setColor(GREEN);
//     } else {
//       setColor(RED);
//     }
//   }
// }

// void drawSetup() {
//   setColor(OFF);
//   byte nb = 0;
//   FOREACH_FACE(f) {
//     if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
//       nb++;
//       setColorOnFace(BLUE, f);
//     }
//   }
//   if (1 == nb) {
//     setColor(GREEN);
//   }
//   if (CONTROLLER == blinkMode) {
//     setColor(MAGENTA);
//   } else if (BRANCH == blinkMode) {
//     setColor(BROWN);
//   }
// }

void drawControllerPulse() {
  //  setColor(MAGENTA);

  int pt = slowControllerPulseTime;
  if (startBeatDrop) {
    pt = fastControllerPulseTime;
  }

  // get progress from 0 - MAX
  long pulseProgress = millis() % pt;

  // transform that progress to a byte (0-255)
  byte pulseMapped = (pulseProgress * 255) / pt;

  // transform that byte with sin
  byte dimness = 0;

  dimness = sin8_C(pulseMapped); // f

  Color c = RED;
  if (!startBeatDrop) {
    if (dimness <= 32) {
      dimness = 0;
      if (wheelCycleFlag) {
        wheelCycle = (wheelCycle + 1) % 6;
        wheelCycleFlag = false;
      }
    } else if (dimness > 200) {
      wheelCycleFlag = true;
    }
    c = colorWheel[wheelCycle];
  }
  setColor(dim(c, dimness));
}
