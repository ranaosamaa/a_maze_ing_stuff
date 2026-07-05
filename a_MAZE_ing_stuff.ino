#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>

#define WHITE SH110X_WHITE
#define BLACK SH110X_BLACK
//display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define X_OFFSET 2
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

//buttons
#define BTN_RIGHT 2
#define BTN_DOWN 3
#define BTN_UP 4
#define BTN_LEFT 5
#define BTN_A 6
#define BTN_QUIT 7

//saving scores using EEPROM
#define ADDR_SNAKE 0
#define ADDR_PONG 4
#define ADDR_FLAPPY 8

//input system
struct Button {
  bool current;
  bool previous;
  bool pressed;
};

Button btnUp, btnDown, btnLeft, btnRight, btnA,  btnQuit;

//game states
enum GameState { MENU, SNAKE, PONG, FLAPPY, GAME_OVER, MAZE, MAZE_TEXT};
GameState gameState = MENU;
GameState lastGame;

unsigned long stateChangeLock = 0;
const int stateLockTime = 200;

//input timing
unsigned long lastInputTime = 0;
const int inputDelay = 120;

bool canInput() { return millis() - lastInputTime > inputDelay; }
bool canChangeState() { return millis() - stateChangeLock > stateLockTime; }

//menu
int menuIndex = 0;
const char menu0[] PROGMEM = "Snake";
const char menu1[] PROGMEM = "Pong";
const char menu2[] PROGMEM = "Flappy";

const char* const menuItems[] PROGMEM = {menu0, menu1, menu2};
float cursorY = 20, targetCursorY = 20;

//game over
int gameOverIndex = 0;
const char go0[] PROGMEM = "Retry";
const char go1[] PROGMEM = "Menu";

const char* const gameOverOptions[] PROGMEM = {go0, go1};

int scrollOffset = 0;

//maze
#define MAZE_W 16
#define MAZE_H 10

// 0 = path, 1 = wall
uint8_t maze[MAZE_H][MAZE_W];

// Player
int playerX = 0;
int playerY = 0;

// Goal
#define GOAL_X (MAZE_W - 1)
#define GOAL_Y (MAZE_H - 1)


struct Cell {
  uint8_t x;
  uint8_t y;
};

const int8_t dirsTemplate[4][2] PROGMEM = {
  {0, -2}, {2, 0}, {0, 2}, {-2, 0}
};

Cell stack[MAZE_W * MAZE_H];
int stackTop = -1;

uint8_t visited[MAZE_H][MAZE_W/8 + 1];

bool isVisited(int x, int y) {
  return visited[y][x >> 3] & (1 << (x & 7));
}

void setVisited(int x, int y) {
  visited[y][x >> 3] |= (1 << (x & 7));
}

void clearVisited() {
  for (int y = 0; y < MAZE_H; y++) {
    for (int i = 0; i < (MAZE_W/8 + 1); i++) {
      visited[y][i] = 0;
    }
  }
}

//snake
struct Segment { uint8_t x, y; } snake[20];
int snakeLength, dx, dy, nextDX, nextDY, foodX, foodY;
int snakeScore = 0, snakeHighScore = 0;
unsigned long snakeTimer = 0;
int snakeSpeed = 200;

//pong
int paddleY, ballX, ballY, ballDX, ballDY;
int pongScore = 0, pongHighScore = 0;
unsigned long pongTimer = 0;
int pongDelay = 35;

//flappy
int birdY, velocity, pipeX;
int gapY;
int flappyScore = 0, flappyHighScore = 0;
unsigned long flappyTimer = 0;
int flappyDelay = 25;

//fps
unsigned long lastFrame = 0;
const int FPS = 30;

extern int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

//input
void readInput() {
  // store previous FIRST
  btnUp.previous = btnUp.current;
  btnDown.previous = btnDown.current;
  btnLeft.previous = btnLeft.current;
  btnRight.previous = btnRight.current;
  btnA.previous = btnA.current;
  btnQuit.previous = btnQuit.current;

  // read new state
  btnUp.current = !digitalRead(BTN_UP);
  btnDown.current = !digitalRead(BTN_DOWN);
  btnLeft.current = !digitalRead(BTN_LEFT);
  btnRight.current = !digitalRead(BTN_RIGHT);
  btnA.current = !digitalRead(BTN_A);
  btnQuit.current = !digitalRead(BTN_QUIT);

  // detect edge
  btnUp.pressed = (btnUp.current && !btnUp.previous);
  btnDown.pressed = (btnDown.current && !btnDown.previous);
  btnLeft.pressed = (btnLeft.current && !btnLeft.previous);
  btnRight.pressed = (btnRight.current && !btnRight.previous);
  btnA.pressed = (btnA.current && !btnA.previous);
  btnQuit.pressed = (btnQuit.current && !btnQuit.previous);
}

void flushInput() {
  btnUp.previous = btnUp.current;
  btnDown.previous = btnDown.current;
  btnLeft.previous = btnLeft.current;
  btnRight.previous = btnRight.current;
  btnA.previous = btnA.current;
  btnQuit.previous = btnQuit.current;
}

//EEPROM
void writeInt(int addr,int v){ EEPROM.put(addr,v); }

int readInt(int addr){
  int v;
  EEPROM.get(addr,v);
  return (v < 0 || v > 10000) ? 0 : v;
}

//setup
void setup() {
  randomSeed(analogRead(A0));

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_QUIT, INPUT_PULLUP);


  Wire.begin();
  Wire.setClock(100000);
  
  u8g2.begin();

  snakeHighScore = readInt(ADDR_SNAKE);
  pongHighScore = readInt(ADDR_PONG);
  flappyHighScore = readInt(ADDR_FLAPPY);
}

//menu
void updateMenu() {

  if(!canChangeState()) return;

  if(btnUp.pressed && canInput()){ menuIndex = (menuIndex + 2) % 3; lastInputTime = millis(); }
  if(btnDown.pressed && canInput()){ menuIndex = (menuIndex + 1) % 3; lastInputTime = millis(); }

  if(btnUp.current && btnDown.current && btnA.current && canChangeState()){
    generateMaze();
    gameState = MAZE;
    stateChangeLock = millis();
    return;
  }

  if(btnA.pressed && canInput()){
    stateChangeLock = millis();
    flushInput();

    if(menuIndex == 0) initSnake();
    if(menuIndex == 1) initPong();
    if(menuIndex == 2) initFlappy();

    lastInputTime = millis();
  }

  targetCursorY = 20 + menuIndex * 15;
  cursorY += (targetCursorY - cursorY) * 0.3;

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_5x7_tr);

    u8g2.drawStr(0 + X_OFFSET, 8, "RANEEM'S ARCADE");
    u8g2.drawLine(0 + X_OFFSET, 10, 128 - X_OFFSET, 10);

    for(int i=0;i<3;i++){
      int y = 20 + i * 15;

      char buffer[10];
      strcpy_P(buffer, (char*)pgm_read_ptr(&menuItems[i]));

      if(i == menuIndex){
        u8g2.drawBox(20 + X_OFFSET, cursorY - 10, 80, 12);
        u8g2.setDrawColor(0);
        u8g2.drawStr(30 + X_OFFSET, cursorY, buffer);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(30 + X_OFFSET, y, buffer);
      }
    }
  } while (u8g2.nextPage());
}


void generateMaze() {

  // Clear maze + visited
  clearVisited();

  for (int y = 0; y < MAZE_H; y++) {
    for (int x = 0; x < MAZE_W; x++) {
      maze[y][x] = 1; // wall
    }
  }

  int startX = 0;
  int startY = 0;

  stackTop = 0;
  stack[0] = { (uint8_t)startX, (uint8_t)startY };

  setVisited(startX, startY);
  maze[startY][startX] = 0;

  while (stackTop >= 0) {

    Cell current = stack[stackTop];

    int8_t dirs[4][2];

    // Copy from PROGMEM
    for (int i = 0; i < 4; i++) {
      dirs[i][0] = pgm_read_byte(&dirsTemplate[i][0]);
      dirs[i][1] = pgm_read_byte(&dirsTemplate[i][1]);
    }

    // Shuffle directions (IMPORTANT for maze randomness)
    for (int i = 0; i < 4; i++) {
      int r = random(4);
      int8_t tx = dirs[i][0];
      int8_t ty = dirs[i][1];
      dirs[i][0] = dirs[r][0];
      dirs[i][1] = dirs[r][1];
      dirs[r][0] = tx;
      dirs[r][1] = ty;
    }

    bool moved = false;

    for (int i = 0; i < 4; i++) {

      int nx = current.x + dirs[i][0];
      int ny = current.y + dirs[i][1];

      if (nx >= 0 && nx < MAZE_W &&
          ny >= 0 && ny < MAZE_H &&
          !isVisited(nx, ny)) {

        setVisited(nx, ny);

        // Remove wall between
        maze[current.y + dirs[i][1]/2][current.x + dirs[i][0]/2] = 0;
        maze[ny][nx] = 0;

        stack[++stackTop] = { (uint8_t)nx, (uint8_t)ny };

        moved = true;
        break;
      }
    }

    if (!moved) {
      stackTop--;
    }
  }

  // Reset player at start
  playerX = 0;
  playerY = 0;

  // Ensure goal is reachable
maze[GOAL_Y][GOAL_X] = 0;

// Also open a neighbor if needed (prevents isolation)
if (GOAL_X > 0) maze[GOAL_Y][GOAL_X - 1] = 0;
else if (GOAL_Y > 0) maze[GOAL_Y - 1][GOAL_X] = 0;
}


void updateMazeGame() {

  // Exit
  if (btnQuit.pressed && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    return;
  }

  // Movement (one step per frame)
  int newX = playerX;
  int newY = playerY;

  if (btnUp.pressed)    newY--;
  if (btnDown.pressed)  newY++;
  if (btnLeft.pressed)  newX--;
  if (btnRight.pressed) newX++;

  // Bounds + collision
  if (newX >= 0 && newX < MAZE_W &&
      newY >= 0 && newY < MAZE_H) {

    if (maze[newY][newX] == 0) {
      playerX = newX;
      playerY = newY;
    }
  }

  // Goal reached
  if (maze[playerY][playerX] == 0 &&
    playerX == GOAL_X && playerY == GOAL_Y) {
    gameState = MAZE_TEXT;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  // Draw
  u8g2.firstPage();
  do {

    int cellW = 128 / MAZE_W;
    int cellH = 64 / MAZE_H;

    for (int y = 0; y < MAZE_H; y++) {
      for (int x = 0; x < MAZE_W; x++) {

        if (maze[y][x] == 1) {
          u8g2.drawBox(
            x * cellW + X_OFFSET,
            y * cellH,
            cellW,
            cellH
          );
        }
      }
    }

    // Player
    u8g2.drawBox(
      playerX * cellW + X_OFFSET + cellW/4,
      playerY * cellH + cellH/4,
      cellW/2,
      cellH/2
    );

// Goal (perfect 5x5 heart, centered)
int gx = GOAL_X * cellW + X_OFFSET;
int gy = GOAL_Y * cellH;

const int size = 5;

// center inside the cell
int hx = gx + (cellW - size) / 2;
int hy = gy + (cellH - size) / 2;

// 5x5 symmetric heart
u8g2.drawPixel(hx+1, hy+0);
u8g2.drawPixel(hx+3, hy+0);

u8g2.drawPixel(hx+0, hy+1);
u8g2.drawPixel(hx+1, hy+1);
u8g2.drawPixel(hx+2, hy+1);
u8g2.drawPixel(hx+3, hy+1);
u8g2.drawPixel(hx+4, hy+1);

u8g2.drawPixel(hx+0, hy+2);
u8g2.drawPixel(hx+1, hy+2);
u8g2.drawPixel(hx+2, hy+2);
u8g2.drawPixel(hx+3, hy+2);
u8g2.drawPixel(hx+4, hy+2);

u8g2.drawPixel(hx+1, hy+3);
u8g2.drawPixel(hx+2, hy+3);
u8g2.drawPixel(hx+3, hy+3);

u8g2.drawPixel(hx+2, hy+4);

  } while (u8g2.nextPage());
}


//maze text
const char longText[] PROGMEM =
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE "
"SECRET MESSAGE ";


void updateMazeText() {

  if ((btnQuit.pressed || btnQuit.current) && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  // -------- Smooth scrolling (momentum-based) --------
  static unsigned long lastScroll = 0;
  static float velocity = 0;
  const int LINE_HEIGHT = 10;

  if (millis() - lastScroll > 16) { // ~60 FPS

  if (btnDown.pressed || btnDown.current) scrollOffset += 30;
  if (btnUp.pressed   || btnUp.current)   scrollOffset -= 30;

    velocity *= 0.85; // friction
    velocity = constrain(velocity, -3, 3); // cap speed

    scrollOffset += velocity;

    lastScroll = millis();
  }

  // -------- Calculate text height ONCE --------
  static int totalHeight = -1;

  if (totalHeight == -1) {
    totalHeight = 0;
    int x_tmp = 0;

    char word_tmp[20];
    uint8_t widx_tmp = 0;

    for (uint16_t i = 0;; i++) {
      char c = pgm_read_byte(&longText[i]);

      if (c == ' ' || c == '\0') {
        word_tmp[widx_tmp] = '\0';

        int w = u8g2.getStrWidth(word_tmp);

        if (x_tmp + w > 128) {
          x_tmp = 0;
          totalHeight += LINE_HEIGHT;
        }

        x_tmp += w + u8g2.getStrWidth(" ");
        widx_tmp = 0;

        if (c == '\0') break;
      } else {
        if (widx_tmp < sizeof(word_tmp) - 1) {
          word_tmp[widx_tmp++] = c;
        }
      }
    }

    totalHeight += LINE_HEIGHT; // final line
  }

  // -------- Clamp scroll properly --------
  int maxScroll = max(0, totalHeight - 64);

  if (scrollOffset < 0) {
    scrollOffset = 0;
    velocity = 0;
  }

  if (scrollOffset > maxScroll) {
    scrollOffset = maxScroll;
    velocity = 0;
  }

  // -------- Render --------
  u8g2.firstPage();
  do {

    int x = 0;
    int y = 0 - scrollOffset;

    char word[20];
    uint8_t widx = 0;

    for (uint16_t i = 0;; i++) {
      char c = pgm_read_byte(&longText[i]);

      if (c == ' ' || c == '\0') {
        word[widx] = '\0';

        int wordWidth = u8g2.getStrWidth(word);

        // Wrap line
        if (x + wordWidth > 128) {
          x = 0;
          y += LINE_HEIGHT;
        }

        // Skip off-screen text (performance boost)
        if (y > -LINE_HEIGHT && y < 64 + LINE_HEIGHT) {
          u8g2.setCursor(x + X_OFFSET, y);
          u8g2.print(word);
        }

        x += wordWidth + u8g2.getStrWidth(" ");
        widx = 0;

        if (c == '\0') break;
      } 
      else {
        if (widx < sizeof(word) - 1) {
          word[widx++] = c;
        }
      }
    }
    if (widx > 0) {
  word[widx] = '\0';

  if (x + u8g2.getStrWidth(word) > 128) {
    x = 0;
    y += LINE_HEIGHT;
  }

  if (y > -LINE_HEIGHT && y < 64) {
    u8g2.setCursor(x + X_OFFSET, y);
    u8g2.print(word);
  }
}

  } while (u8g2.nextPage());
}

//snake
void spawnFood(){
  bool ok;
  do{
    ok = true;
    foodX = random(0,20);
    foodY = random(0,10);
    for(int i=0;i<snakeLength;i++)
      if(snake[i].x == foodX && snake[i].y == foodY) ok = false;
  }while(!ok);
}
void initSnake(){
  snakeLength = 3;
  snakeScore = 0;
  dx = 1; dy = 0;
  nextDX = 1, nextDY = 0;

  snake[0] = {10,5};
  snake[1] = {9,5};
  snake[2] = {8,5};

  spawnFood();
  gameState = SNAKE;
  stateChangeLock = millis();
}

void updateSnake(){

  if (btnQuit.pressed && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  if(btnUp.current && nextDY == 0){ nextDX = 0; nextDY = -1; }
  if(btnDown.current && nextDY == 0){ nextDX = 0; nextDY = 1; }
  if(btnLeft.current && nextDX == 0){ nextDX = -1; nextDY = 0; }
  if(btnRight.current && nextDX == 0){ nextDX = 1; nextDY = 0; }

  snakeSpeed = max(100, 180 - snakeScore * 3);

  if(millis() - snakeTimer >= snakeSpeed) {
    snakeTimer = millis();

    dx = nextDX;
    dy = nextDY;

    for(int i=snakeLength-1;i>0;i--) snake[i] = snake[i-1];
    snake[0].x += dx;
    snake[0].y += dy;
  }

  if(snake[0].x<0||snake[0].x>=20||snake[0].y<0||snake[0].y>=10){
    endGame(snakeScore,snakeHighScore,ADDR_SNAKE);
    return;
  }

  for(int i=1;i<snakeLength;i++)
    if(snake[0].x==snake[i].x&&snake[0].y==snake[i].y){
      endGame(snakeScore,snakeHighScore,ADDR_SNAKE);
      return;
    }

  if(snake[0].x==foodX&&snake[0].y==foodY){
    snakeLength++;
    snakeScore++;
    spawnFood();
  }

  u8g2.firstPage();
  do {
    char buf[16];

    sprintf(buf,"S:%d",snakeScore);
    u8g2.drawStr(0 + X_OFFSET,8,buf);

    sprintf(buf,"H:%d",snakeHighScore);
    u8g2.drawStr(70 + X_OFFSET,8,buf);

    u8g2.drawFrame(0 + X_OFFSET,10,124,54);

    for(int i=0;i<snakeLength;i++)
      u8g2.drawBox(snake[i].x*6 + X_OFFSET, snake[i].y*6+10, 5, 5);

    u8g2.drawBox(foodX*6 + X_OFFSET, foodY*6+10, 5, 5);

  } while (u8g2.nextPage());
}
//pong
void initPong(){
  paddleY = 25;
  ballX = 64;
  ballY = 32;
  ballDX = (random(0,2) == 0) ? 2 : -2;;
  ballDY = random(-2,3);
  if(ballDY == 0) ballDY = 1;
  pongScore = 0;
  gameState = PONG;
  stateChangeLock = millis();
}

void updatePong(){

  if (btnQuit.pressed && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  if(millis() - pongTimer >= pongDelay){
    pongTimer = millis();

    if(btnUp.current) paddleY -= 5;
    if(btnDown.current) paddleY += 5;
    paddleY = constrain(paddleY, 0, 52);

    ballX += ballDX;
    ballY += ballDY;
  }

  if(ballY <= 0 || ballY >= 61) ballDY *= -1;
  if(ballX >= 125) ballDX *= -1;

  if(ballX <= 6) {
    if(ballY >= paddleY && ballY <= paddleY + 12) {
      ballDX *= -1;
      ballDY += (ballY - (paddleY + 6)) * 0.2 + random(-1,2);
      ballDY = constrain(ballDY, -3, 3);
      if(abs(ballDY) < 1) ballDY = (ballDY >= 0) ? 1 : -1;
      pongScore++;
    } else {
      endGame(pongScore, pongHighScore, ADDR_PONG);
      return;
    }
  }

  u8g2.firstPage();
  do {
    char buf[16];

    sprintf(buf,"S:%d",pongScore);
    u8g2.drawStr(0 + X_OFFSET,8,buf);

    sprintf(buf,"H:%d",pongHighScore);
    u8g2.drawStr(70 + X_OFFSET,8,buf);

    u8g2.drawBox(2 + X_OFFSET, paddleY, 3, 12);
    u8g2.drawBox(ballX + X_OFFSET, ballY, 3, 3);

  } while (u8g2.nextPage());
}
//flappy
void initFlappy(){
  birdY=30;
  velocity=0;
  pipeX=120;
  gapY=25;
  flappyScore=0;
  gameState=FLAPPY;
  stateChangeLock = millis();
}

void updateFlappy(){

  if (btnQuit.pressed && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  if(millis() - flappyTimer >= flappyDelay){
    flappyTimer = millis();

    if(btnA.pressed) velocity = -4;

    velocity += 1.0;
    velocity = constrain(velocity, -5, 5);
    birdY += velocity;

    pipeX -= 2;

    if(pipeX < 0){ 
      pipeX = 128; 
      gapY = random(10,40); 
      flappyScore++; 
    }

    bool hit = (pipeX<24&&pipeX>16)&&(birdY<gapY||birdY>gapY+20);

    if(birdY<0||birdY>63||hit){
      endGame(flappyScore,flappyHighScore,ADDR_FLAPPY);
      return;
    }
  }

  u8g2.firstPage();
  do {
    char buf[16];

    sprintf(buf,"S:%d",flappyScore);
    u8g2.drawStr(0 + X_OFFSET,8,buf);

    sprintf(buf,"H:%d",flappyHighScore);
    u8g2.drawStr(70 + X_OFFSET,8,buf);

    u8g2.drawBox(20 + X_OFFSET,birdY,4,4);
    u8g2.drawBox(pipeX + X_OFFSET,0,8,gapY);
    u8g2.drawBox(pipeX + X_OFFSET,gapY+20,8,64);

  } while (u8g2.nextPage());
}

//game over
void endGame(int s,int &h,int addr){
  if(s>h){ h=s; writeInt(addr,h); }
  
  lastGame = gameState;
  gameOverIndex = 0;
  gameState = GAME_OVER;

  stateChangeLock = millis();
  lastInputTime = millis();
  flushInput();
}

void updateGameOver(){

  if (btnQuit.pressed && canChangeState()) {
    gameState = MENU;
    stateChangeLock = millis();
    lastInputTime = millis();
    flushInput();
    return;
  }

  if(btnUp.pressed && canInput()){
    gameOverIndex = (gameOverIndex + 1) % 2;
    lastInputTime = millis();
  }

  if(btnDown.pressed && canInput()){
    gameOverIndex = (gameOverIndex + 1) % 2;
    lastInputTime = millis();
  }

  if(btnA.pressed && canChangeState()){
    stateChangeLock = millis();

    if(gameOverIndex == 0){
      flushInput();
      if(lastGame == SNAKE) initSnake();
      if(lastGame == PONG) initPong();
      if(lastGame == FLAPPY) initFlappy();
    } else {
      gameState = MENU;
      stateChangeLock = millis();
      lastInputTime = millis(); 
      flushInput();
    }
  }

  u8g2.firstPage();
  do {
    u8g2.drawStr(25 + X_OFFSET,10,"GAME OVER");

    for(int i=0;i<2;i++){
      int y = 30 + i*15;

      char buffer[10];
      strcpy_P(buffer, (char*)pgm_read_ptr(&gameOverOptions[i]));

      if(i == gameOverIndex){
        u8g2.drawBox(20 + X_OFFSET,y-10,80,12);
        u8g2.setDrawColor(0);
        u8g2.drawStr(30 + X_OFFSET,y,buffer);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(30 + X_OFFSET,y,buffer);
      }
    }

  } while (u8g2.nextPage());
}

//loop
void loop(){
  readInput();

  unsigned long now = millis();

  if(now - lastFrame >= 1000/FPS){
    lastFrame = now;

    switch(gameState){
      case MENU: updateMenu(); break;
      case SNAKE: updateSnake(); break;
      case PONG: updatePong(); break;
      case FLAPPY: updateFlappy(); break;
      case GAME_OVER: updateGameOver(); break;
      case MAZE: updateMazeGame(); break;
      case MAZE_TEXT: updateMazeText(); break;
    }

    flushInput();
  }
}