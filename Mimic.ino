#include <blinklib.h>

//#include "Serial.h"

#define BROWN dim(ORANGE, 100)

enum signalStates {
  SETUP,             // 0 , 0
  GAME,              // 1 , 4
  GAME_EVEN,         // 1 , 4
  GAMEOVER_LOSE,     // 2, 8
  GAMEOVER_WIN,      // 3, 12
  SHITSTORM,         // 4, 16
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
// random 1-5) #define startRandomiserSize 9 #define shitStormLength 3 //number
// of shitstorm itterations #define shitStormWaitTimeModifier 3000 //multiply by
// 15-20

//
////---Low Intensity---
//#define GAMELENGTH 5000  //sets the time to complete the task
//#define startMultiplier 1000 //mutplier for the time between tasks (against a
//// random 1-5)
//#define startRandomiserSize 4
//#define shitStormLength 3 //number of shitstorm itterations
//#define shitStormWaitTimeModifier 1000 //multiply by 15-20

//---Medium Intensity---
#define GAMELENGTH 2000
#define startMultiplier 750
#define startRandomiserSize 4
#define shitStormLength 4
#define shitStormWaitTimeModifier 1000 // multiply by 15-20

 byte level = 0;
#define MAX_LEVEL 20

//---High Intensity---
//#define GAMELENGTH 2000
//#define startMultiplier 500
//#define startRandomiserSize 4
//#define shitStormLength 4
//#define shitStormWaitTimeModifier 750 //multiply by 15-20

#define shitStormWarnTime 4000
#define shitStormResetTime 3000

#define failLength 1000

#define slowControllerPulseTime 2000
#define fastControllerPulseTime 500
 byte countShitStorm = 0;
 bool startShitStorm = false;

 byte countNeighbours = 0;
 byte connections = 0;

void setup() {
  randomize();
  setColor(OFF);
  setValueSentOnAllFaces(INERT << 2);
  gameTimer.never();
  startTimer.never();
  missingPlayerTimer.never();
  //  sp.begin();
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
        if (connectedFace != f) {
          changeConnectedFace(f);
        }
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
    switch (llval) {
    case LL_INERT:
      LLInertLoop(f);
      break;
    case LL_LOSE_LIFE:
      LLSendLoop(f);
      break;
    case LL_REVERT:
      LLResolveLoop(f);
      break;
    }
  }
  long pulseProgress = millis() % PULSE_LENGTH;
  byte pulseMapped = (pulseProgress * 255) / PULSE_LENGTH;

  if (SETUP == gameMode) {
    setupLoop();
    drawSetup();
  } else if (GAME == gameMode) {

    gameLoop();
    if (GAMEPLAYER == blinkMode) {
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
        drawDoubleClick();
        break;
      case TRIPLE_C:
        drawTripleClick();
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
      gameOverLoop();
    }
    drawGameOver();
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
      if (BRANCH == blinkMode || CONNECTOR == blinkMode) {
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
        if (!evenSent && !isValueReceivedOnFaceExpired(f)) {
          if ((connectedFace + 3) % 6 == f || (connectedFace + 2) % 6 == f ||
              (connectedFace + 4) % 6 == f) {
            sendData = GAME_EVEN;
            evenSent = true;
          }
        } else if (!isValueReceivedOnFaceExpired(f)) {
          evenSent = false;

        }
      }
    }

    sendData = sendData << 2;

    sendData = sendData + lifeLostState[f];

    setValueSentOnFace(sendData, f);
  }

  
  
  
}

 void inertLoop() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      byte payload = getPayload(f);
      if (INERT > val) {
        switch (val) {
        case SETUP:
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
        case SHITSTORM:
          receivedShitShow(f);
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
        allSend = false;
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
    // prevent branches from moving to Inert until all non-connectedFace blinks
    // are Intert
    if (!anyResolve) {
      signalState = INERT;
    }
  }
}

 void LLInertLoop(byte face) {
  if (!isValueReceivedOnFaceExpired(face)) {
    byte payload = getPayload(getLastValueReceivedOnFace(face));
    if (LL_LOSE_LIFE == payload) {
      receivedLLLoseLife(face);
    } else if (LL_INERT == payload && connectedFace == face &&
               lifeLossCounter > 0) {
      lifeLostState[face] = LL_LOSE_LIFE;
      --lifeLossCounter;
    }
  }
}

 void LLSendLoop(byte face) {
  if (!isValueReceivedOnFaceExpired(face)) {
    byte payload = getPayload(getLastValueReceivedOnFace(face));
    if (LL_INERT != payload) {
      lifeLostState[face] = LL_REVERT;
    }
  }
}

 void LLResolveLoop(byte face) {
  if (!isValueReceivedOnFaceExpired(face)) {
    byte payload = getPayload(getLastValueReceivedOnFace(face));
    if (LL_LOSE_LIFE != payload) {
      lifeLostState[face] = LL_INERT;
    }
  }
}

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
  countShitStorm = 0;
  startShitStorm = false;
  score = 0;
  signalState = SETUP;
  gameMode = SETUP;
  // connectedFace = 7;
  targetFace = 7;
  gameTimer.never();
  startTimer.never();
  connections = 0;
  FOREACH_FACE(f) { lifeLostState[f] = LL_INERT; }
  lifeLossCounter = 0;
  if (BRANCH > blinkMode) {
    blinkMode = UNUSED;
  }
  level = 0;
  evenSent = true;
  amEven = false;
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

 void receivedShitShow(byte face) {
  signalState = SHITSTORM;
  kickoffShitStorm();
}

 void kickoffShitStorm() {
  startShitStorm = true;
  if (CONNECTOR == blinkMode || BRANCH == blinkMode) {
    gameTimer.set(GAMELENGTH * 4);
  }
}

 void setupLoop() {
  if (CONTROLLER == blinkMode) {
    if (buttonDoubleClicked() && INERT == signalState) {
      signalState = GAME;
      gameMode = GAME;
      startTimer.set(4000);
      startShitStorm = false;
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
  if (countShitStorm > 0) {
    countShitStorm++;
  }
  if (countShitStorm > 0 && getShitStormLength() >= countShitStorm) {
    startTimer.set(100);
  } else {
    countShitStorm = 0;
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
        return startMultiplier * (random(2) + 3); // odd or even would be better
      case(1):
        return startMultiplier * (random(startRandomiserSize) + 3);
        break;
      case(2):
        return startMultiplier * (random(startRandomiserSize) + 1);
        break;
      case(3):
        return startMultiplier * 2;
        break;
      case(4):
        return startMultiplier;
        break;
    }
    */
}

 byte getShitStormLength() {
  if (level < MAX_LEVEL) {
    return shitStormLength;
  }
  return shitStormLength + 2;
}

 void failGame() {
  //  countShitStorm = 0;
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
    if (startTimer.isExpired() || (startShitStorm && WAIT == game)) {
      if (startShitStorm) {
        countShitStorm++;
        startShitStorm = false;
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
        // start shitstorm timer
        gameTimer.set(shitStormWaitTimeModifier * (random(5) + 15));
        startTimer.never();
      } else if (gameTimer.isExpired() && !startShitStorm) {
        // start flashing
        startShitStorm = true;
        gameTimer.set(shitStormWarnTime);
      } else if (gameTimer.isExpired() & startShitStorm &&
                 INERT == signalState) {
        // send shitstorm instruction, stop flashing, wait until shitstorm might
        // be finished
        signalState = SHITSTORM;
        startShitStorm = false;
        gameTimer.never();
        startTimer.set(shitStormResetTime);
      }
    }

    if (buttonDoubleClicked()) {
      receivedSetup();
    }
  }
}

void gameOverLoop() {
  if (buttonDoubleClicked()) {
    receivedSetup();
  }
}


byte getSignalState(byte data) { return ((data >> 2)); }

byte getPayload(byte data) { return (data & 3); }

void changeConnectedFace(byte newFace) {
  lifeLostState[newFace] = lifeLostState[connectedFace];
  lifeLostState[connectedFace] = LL_INERT;
  connectedFace = newFace;
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

void drawSingleClick() {
  setColor(OFF);
  if (millis() % 650 > 400) {
    setColor(BLUE);
  }
}

void drawDoubleClick() {
  setColor(OFF);
  int mils = millis() % 1000;
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
}

void drawTripleClick() {
  setColor(OFF);
  int mils = millis() % 1350;
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
}

void drawScore(bool isBranch, byte pulseMapped) {
  if (startShitStorm) {
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
        dimness = sin8_C(pulseMapped + (42 * f * rotation)); // f
        setColorOnFace(dim(RED, dimness), ((f + connectedFace) % 6));
      }
    } else {
      startShitStorm = false;
      gameTimer.never();
    }
  } else {
    if (!isBranch) {
      setColor(OFF);
      for (byte i = score-1; i >=0 ; --i) {
        setColorOnFace(RED, i);
      }
    } else {
      setColor(BROWN);
    }
  }
}

void drawGameOver() {
  if (CONTROLLER == blinkMode) {
    setColor(OFF);
    setColorOnFace(RED, gameOverFace);
  } else {
    if (GAMEOVER_WIN == gameMode) {
      setColor(GREEN);
    } else {
      setColor(RED);
    }
  }
}

 void drawSetup() {
  setColor(OFF);
  byte nb = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
      nb++;
      setColorOnFace(BLUE, f);
    }
  }
  if (1 == nb) {
    setColor(GREEN);
  }
  if (CONTROLLER == blinkMode) {
    setColor(MAGENTA);
  } else if (BRANCH == blinkMode) {
    setColor(BROWN);
  }
}

 void drawControllerPulse() {
  // setColor(MAGENTA);

  int pt = slowControllerPulseTime;
  if (startShitStorm) {
    pt = fastControllerPulseTime;
  }

  // get progress from 0 - MAX
  long pulseProgress = millis() % pt;

  // transform that progress to a byte (0-255)
  byte pulseMapped = (pulseProgress * 255) / pt;

  // transform that byte with sin
  byte dimness = 0;

  dimness = sin8_C(pulseMapped); // f
  Color c = MAGENTA;
  if (startShitStorm) {
    c = RED;
  }
  setColor(dim(c, dimness));
}
