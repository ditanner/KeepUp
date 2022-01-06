
#include "Serial.h"

enum signalStates {SETUP, GAME, GAMEOVER, LIFELOSS, INERT, RESOLVE};
byte signalState = INERT;
byte gameMode = SETUP;

enum games {WAIT, FAIL, ROTATE_CW, ROTATE_ACW, FLIP, DOUBLE_C, TRIPLE_C};
byte game = WAIT;

enum blinkModes {UNUSED, GAMEPLAYER, CONNECTOR, SPUR, CONTROLLER};
byte blinkMode = UNUSED;

enum gameOverStates {GO_WIN, GO_LOSS};
byte gameOverState = GO_WIN;

byte connectedFace = 7;
byte targetFace = 7;

byte score = 0;

int gameOverFace = -1;
bool gameOver = false;
bool gameOverPublished = false;



#define PULSE_LENGTH 750

//ServicePortSerial sp;

Timer gameTimer;
Timer startTimer;

//---Low Intensity---
//#define gameLength 5000
//#define startMultiplier 1000

//---Medium Intensity---
//#define gameLength 5000
//#define startMultiplier 500

//---High Intensity---
#define gameLength 2500
#define startMultiplier 500

#define failLength 1000


void setup() {
  randomize();
  setColor(OFF);
  setValueSentOnAllFaces(INERT << 2);
  //  sp.begin();
}

void loop() {

  if (CONTROLLER == blinkMode && INERT == signalState && gameOver && !gameOverPublished) {
    signalState = GAMEOVER;
    gameMode = GAMEOVER;
    gameOverPublished = true;
  }

  if (signalState < INERT) {
    sendLoop();
  } else if (INERT == signalState ) {
    inertLoop();
  } else if (RESOLVE == signalState) {
    resolveLoop();
  }

  if (SETUP == gameMode) {
    setupLoop();
    drawSetup();
  } else if (GAME == gameMode && INERT <= signalState) {

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
          drawRotate(true);
          break;
        case ROTATE_ACW:
          drawRotate(false);
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
    } else if (GAMEPLAYER < blinkMode) {
      drawScore();
      //      if (LIFELOSS == signalState) {
      //        setColor(RED);
      //      }
    } else {
      setColor(OFF);
    }
  } else if (GAMEOVER == gameMode) {
    if (CONTROLLER == blinkMode) {
      gameLoop();
    }
    drawGameOver();

  }


  //dump button data
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonPressed();
  buttonMultiClicked();

  byte sendData = 0;

  sendData = (signalState << 2);
  //sendData = sendData + gameMode
  if (GAMEOVER == signalState) {
    if (CONTROLLER == blinkMode) {
      FOREACH_FACE(f) {
        if (f == gameOverFace) {
          setValueSentOnFace(sendData + 1, f);
        } else {
          setValueSentOnFace(sendData, f);
        }
      }
    } else {
      setValueSentOnAllFaces(sendData + gameOverState);
    }
  } else {
    setValueSentOnAllFaces(sendData);
  }

}


void inertLoop() {
  bool sendReceived = false;
  byte val = 0;
  byte gameOverCount = 0;
  FOREACH_FACE(f) {
    if ( !sendReceived) {
      if (!isValueReceivedOnFaceExpired(f) ) {//a neighbor!
        val = getSignalState(getLastValueReceivedOnFace(f));
        if (val < INERT ) {//a neighbor saying SEND!
          sendReceived = true;
          //          sp.print("SEND Received - moving to mode ");
          //          sp.println(val);
          if (SETUP == val) {
            receivedSetup(f);
          } else if ( GAME == val) {
            receivedGame(f);
          }
          else if ( LIFELOSS == val) {
            //            sp.println("HELLO!!!");
            if (CONTROLLER == blinkMode) {
              //              sp.println("Moving to RESOLVE");
              gameOverCount++;
              if (gameOverCount > 1) {
                gameOverFace = -1; //it's a tie - keep going
                signalState = RESOLVE;
                gameOver = false;
              } else {
                gameOverFace = f;
                signalState = RESOLVE;
                gameOver = true;
              }
              //GAMEOVER!!!
            } else if (GAMEPLAYER < blinkMode && f != connectedFace) {
              //              sp.println("Moving to LIFELOSS");
              if (score < 6) {
                score++;
                signalState = RESOLVE;
                sendReceived = false; // in case there are more than one sending a score through
              } else {
                signalState = val;
              }
            } else {
              sendReceived = false;
            }

          } else if (GAMEOVER == val) {
            if (SETUP < signalState) {
              gameOverState = getPayload(getLastValueReceivedOnFace(f));
              signalState = GAMEOVER;
              gameMode = GAMEOVER;
            } else {
              sendReceived = false;
            }
          }
        }
      }
    }
  }
}

void receivedSetup(byte face) {
  gameOver = false;
  gameOverPublished = false;
  gameOverState = 0;
  score = 0;
  signalState = SETUP;
  gameMode = SETUP;
  connectedFace = 7;
  targetFace = 7;
  gameTimer.never();
  startTimer.never();
}

void receivedGame(byte face) {
  byte neighbours = 0;
  FOREACH_FACE(d) {
    if (!isValueReceivedOnFaceExpired(d)) {
      neighbours++;
    }
  }
  signalState = GAME;
  gameMode = GAME;
  connectedFace = face;
  if (1 == neighbours) {
    blinkMode = GAMEPLAYER;
    startTimer.set(startMultiplier * (random(4) + 2));
    game = WAIT;
    //              sp.println("game is 0");
    gameTimer.never();
  } else if (2 == neighbours) {
    blinkMode = CONNECTOR;
  } else if (3 == neighbours) {
    blinkMode = SPUR;
  }

}

void sendLoop() {
  bool allSend = true;
  bool allGameOver = true;
  byte countReceivers = 0;
  //look for neighbors who have not heard the GO news
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      countReceivers++;
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      if (GAMEOVER != val) {
        allGameOver = false;
      }
      if (val == INERT && (LIFELOSS != signalState || f == connectedFace || GAMEPLAYER == blinkMode)) {//This neighbor doesn't know it's SEND time. Stay in SEND, uless we're ignoring it it
        allSend = false;
      } else if (GAMEOVER == val && GAMEOVER != gameMode) {
        allSend = false;
        gameOverState = getPayload(getLastValueReceivedOnFace(f));
        signalState = GAMEOVER;
        gameMode = GAMEOVER;
      } else {
        if (INERT > signalState && INERT > val && val != signalState) {
          //conflict!!!
          if (signalState > val) {
            //            sp.println("CONFLICT!");
            if ( SETUP == val) {
              receivedSetup(f);
            }
            return;
          }
        }
      }
    }
  }
  if (allSend && countReceivers > 0) {
    //    sp.println("All Resolve");
    if (signalState != GAMEOVER || allGameOver) {
      signalState = RESOLVE;
    }
  }
}

void resolveLoop() {
  bool allResolve = true;

  //look for neighbors who have not moved to RESOLVE
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      byte val = getSignalState(getLastValueReceivedOnFace(f));
      if (val < INERT) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
        allResolve = false;
      }
      if (GAMEOVER == val && GAMEOVER != gameMode) {
        allResolve = false;
        gameOverState = getPayload(getLastValueReceivedOnFace(f));
        signalState = GAMEOVER;
        gameMode = GAMEOVER;
      }
    }
  }
  if (allResolve) {
    //    sp.println("All INERT");
    signalState = INERT;
  }
}

void setupLoop() {
  if (buttonDoubleClicked()) {
    signalState = GAME;
    gameMode = GAME;
    blinkMode = CONTROLLER;
  }
}

void gameLoop() {
  if (GAMEPLAYER == blinkMode) {
    byte currentFace = 7;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        currentFace = f;
      }
    }
    if (startTimer.isExpired()) {
      gameTimer.set(gameLength);
      game = random(4) + 2;
      //     sp.print("180 game is ");
      //     sp.println(game);
      startTimer.never();
      connectedFace = currentFace;

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
        game = FAIL;
        //        sp.print("203 game is ");
        //        sp.println(game);
        gameTimer.set(failLength);
        signalState = LIFELOSS;
        return;
      } else {
        if (LIFELOSS == signalState) { //lifeloss hasn't been received
          gameTimer.set(failLength);
          return;
        }
      }
      startTimer.set(startMultiplier * (random(4) + 1));
      game = WAIT;
      //      sp.print("210 game is ");
      //      sp.println(game);
      gameTimer.never();
      return;
    }
    if (!gameTimer.isExpired() && FAIL != game && WAIT != game) {
      byte clicks = 0;
      if (buttonMultiClicked()) {
        clicks = buttonClickCount();
      }
      bool sglClick = buttonSingleClicked();
      bool dblClick = buttonDoubleClicked();



      if (currentFace != connectedFace && currentFace != targetFace && currentFace != 7) {
        game = FAIL;
        //        sp.print("224 game is ");
        //        sp.println(game);

        gameTimer.set(failLength);
        signalState = LIFELOSS;

        return;
      } else if (game <= 4 && (clicks > 0 || dblClick || sglClick)) {
        game = FAIL;
        //       sp.print("231 game is ");
        //        sp.println(game);

        gameTimer.set(failLength);
        signalState = LIFELOSS;

        return;
      }
      switch (game) {
        case ROTATE_CW:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            //            sp.print("242 game is ");
            //           sp.println(game);

            gameTimer.never();
          }
          break;
        case ROTATE_ACW:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            //            sp.print("252 game is ");
            //            sp.println(game);

            gameTimer.never();
          }
          break;
        case FLIP:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            //            sp.print("261 game is ");
            //            sp.println(game);

            gameTimer.never();
          }
          break;
        case DOUBLE_C:
          if (clicks > 2 || sglClick) {
            game = FAIL;
            gameTimer.set(failLength);
            signalState = LIFELOSS;

            return;
          }
          if (dblClick) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            //            sp.print("271 game is ");
            //            sp.println(game);

            gameTimer.never();
          }
          break;
        case TRIPLE_C:
          if (dblClick || sglClick) {
            game = FAIL;
            gameTimer.set(failLength);
            signalState = LIFELOSS;

            return;
          }
          if (3 == clicks) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            //            sp.print("282 game is ");
            //            sp.println(game);

            gameTimer.never();
          }
      }
    }
  } else {
    if (buttonDoubleClicked()) {
      gameOver = false;
      gameOverPublished = false;
      gameOverState = 0;
      score = 0;
      signalState = SETUP;
      gameMode = SETUP;
      connectedFace = 7;
      targetFace = 7;
      gameTimer.never();
      startTimer.never();

    }
  }
}

byte stepOverFace(byte face, byte steps) {
  face = face + steps;
  return face % 6;
}

byte getSignalState(byte data) {
  return ((data >> 2) );
}

byte getPayload(byte data) {
  return (data & 3);
}

void drawRotate(bool clockwise) {
  //setColor(MAGENTA);

  //get progress from 0 - MAX
  int pulseProgress = millis() % PULSE_LENGTH;

  //transform that progress to a byte (0-255)
  byte pulseMapped = map(pulseProgress, 0, PULSE_LENGTH, 0, 255);

  //transform that byte with sin
  byte dimness = 0;

  //set color
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
  //setColor(MAGENTA);

  //get progress from 0 - MAX
  int pulseProgress = millis() % PULSE_LENGTH;

  //transform that progress to a byte (0-255)
  byte pulseMapped = map(pulseProgress, 0, PULSE_LENGTH, 0, 255);

  //transform that byte with sin
  byte dimness = 0;

  int rotation = 1;

  //set color
  FOREACH_FACE(f) {
    if (f < 3) {
      rotation = -1;
    } else {
      rotation = 1;
    }
    dimness = sin8_C(pulseMapped + (42 * f * rotation)); //f
    setColorOnFace(dim(YELLOW, dimness), ((f+connectedFace)%6));
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
  if (millis() % 1000 > 750) {
    //    setColor(BLUE);
    setColorOnFace(BLUE, 0);
    setColorOnFace(BLUE, 1);
    setColorOnFace(BLUE, 2);
  } else if (millis() % 1000 > 400 && millis() % 1000 <= 650) {
    //    setColor(BLUE);
    setColorOnFace(BLUE, 3);
    setColorOnFace(BLUE, 4);
    setColorOnFace(BLUE, 5);
  }

}

void drawTripleClick() {
  setColor(OFF);
  if (millis() % 1350 > 1100) {
    //    setColor(BLUE);
    setColorOnFace(BLUE, 0);
    setColorOnFace(BLUE, 1);
  } else if (millis() % 1350 > 400 && millis() % 1350 <= 650) {
    //    setColor(BLUE);
    setColorOnFace(BLUE, 2);
    setColorOnFace(BLUE, 3);
  } else if (millis() % 1350 > 750 && millis() % 1350 <= 1000) {
    //    setColor(BLUE);
    setColorOnFace(BLUE, 4);
    setColorOnFace(BLUE, 5);
  }
}

void drawScore() {
  setColor(OFF);
  for (byte i = 0; i < score; i++) {
    setColorOnFace(RED, i);
  }
}

void drawGameOver() {
  if (CONTROLLER == blinkMode) {
    setColor(OFF);
    setColorOnFace(RED, gameOverFace);
  } else {
    if (GO_WIN == gameOverState) {
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
    if (!isValueReceivedOnFaceExpired(f) ) {//a neighbor!
      nb++;
      setColorOnFace(BLUE, f);
    }
  }
  if (1 == nb) {
    setColor(GREEN);
  }
  if (CONTROLLER == blinkMode) {
    setColor(MAGENTA);
  }
}
