
#include "Serial.h"

enum signalStates {SETUP, GAME, INERT, RESOLVE};
byte signalState = INERT;
byte gameMode = SETUP;

enum games {WAIT, FAIL, ROTATE_CW, ROTATE_ACW, FLIP, DOUBLE_C, TRIPLE_C};
byte game = WAIT;

enum blinkModes {UNUSED, GAMEPLAYER, SCORER};
byte blinkMode = UNUSED;

byte connectedFace = 7;
byte targetFace = 7;

#define PULSE_LENGTH 750

ServicePortSerial sp;

Timer gameTimer;
#define gameLength 5000

Timer startTimer;
#define startMultiplier 1000

void setup() {
  randomize();
  setColor(OFF);
  sp.begin();
}

void loop() {

  if (signalState < INERT) {
    sendLoop();
  } else if (INERT == signalState ) {
    inertLoop();
  } else if (RESOLVE == signalState) {
    resolveLoop();
  }

  switch (gameMode) {
    case SETUP:
      setupLoop();
      break;
    case GAME:
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
      } else {
        setColor(OFF);
      }
      break;
  }

  //dump button data
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonPressed();
  buttonMultiClicked();

  byte sendData = 0;

  sendData = (signalState << 3);
  setValueSentOnAllFaces(sendData);

}


void inertLoop() {
  bool sendReceived = false;
  byte val = 0;
  FOREACH_FACE(f) {
    if ( !sendReceived) {
      if (!isValueReceivedOnFaceExpired(f) ) {//a neighbor!
        val = getSignalState(getLastValueReceivedOnFace(f));
        if (val < INERT ) {//a neighbor saying SEND!
          sp.print("SEND Received - moving to mode ");
          sp.println(val);
          switch (val) {
            case SETUP:
              signalState = val;
              gameMode = val;
              connectedFace = 7;
              targetFace = 7;
              gameTimer.never();
              startTimer.never();
              break;
            case GAME:
              signalState = val;
              gameMode = val;
              blinkMode = GAMEPLAYER;
              connectedFace = f;
              startTimer.set(startMultiplier * (random(4) + 2));
              game = WAIT;
              sp.println("game is 0");
              gameTimer.never();
              break;
          }
          sendReceived = true;
        }
      }
    }
  }
}

void sendLoop() {
  bool allSend = true;
  //look for neighbors who have not heard the GO news
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == INERT) {//This neighbor doesn't know it's SEND time. Stay in SEND
        allSend = false;
      }
    }
  }
  if (allSend) {
    sp.println("All Resolve");
    signalState = RESOLVE;
  }
}

void resolveLoop() {
  bool allResolve = true;

  //look for neighbors who have not moved to RESOLVE
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) < INERT) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
        allResolve = false;
      }
    }
  }
  if (allResolve) {
    sp.println("All INERT");
    signalState = INERT;
  }
}

void setupLoop() {
  if (buttonDoubleClicked()) {
    signalState = GAME;
    gameMode = GAME;
    blinkMode = SCORER;
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
      sp.print("180 game is ");
      sp.println(game);
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
        sp.print("203 game is ");
        sp.println(game);
        gameTimer.set(1000);
        return;
      }
      startTimer.set(startMultiplier * (random(4) + 1));
      game = WAIT;
      sp.print("210 game is ");
      sp.println(game);
      gameTimer.never();
      return;
    }
    if (!gameTimer.isExpired() && FAIL != game && WAIT != game) {
      byte clicks = 0;
      if(buttonMultiClicked()) {
        clicks = buttonClickCount();
      }
      bool sglClick = buttonSingleClicked();
      bool dblClick = buttonDoubleClicked();



      if (currentFace != connectedFace && currentFace != targetFace && currentFace != 7) {
        game = FAIL;
        sp.print("224 game is ");
        sp.println(game);

        gameTimer.set(1000);
        return;
      } else if (game < 4 && (clicks > 0 || dblClick || sglClick)) {
        game = FAIL;
        sp.print("231 game is ");
        sp.println(game);

        gameTimer.set(1000);
        return;
      }
      switch (game) {
        case ROTATE_CW:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            sp.print("242 game is ");
            sp.println(game);

            gameTimer.never();
          }
          break;
        case ROTATE_ACW:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            sp.print("252 game is ");
            sp.println(game);

            gameTimer.never();
          }
          break;
        case FLIP:
          if (!isValueReceivedOnFaceExpired(targetFace)) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            sp.print("261 game is ");
            sp.println(game);

            gameTimer.never();
          }
          break;
        case DOUBLE_C:
          if (dblClick) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            sp.print("271 game is ");
            sp.println(game);

            gameTimer.never();
          }
          break;
        case TRIPLE_C:
          if (3 == clicks) {
            startTimer.set(startMultiplier * (random(4) + 1));
            game = WAIT;
            sp.print("282 game is ");
            sp.println(game);

            gameTimer.never();
          }
      }
    }
  } else {
    if (buttonDoubleClicked()) {
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
  return ((data >> 3) );
}

byte getPayload(byte data) {
  return (data & 7);
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
    dimness = sin8_C(pulseMapped + (42 * f * rotation));
    setColorOnFace(dim(YELLOW, dimness), f);
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
