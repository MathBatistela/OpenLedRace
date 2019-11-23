/*  
 * ____                     _      ______ _____    _____
  / __ \                   | |    |  ____|  __ \  |  __ \               
 | |  | |_ __   ___ _ __   | |    | |__  | |  | | | |__) |__ _  ___ ___ 
 | |  | | '_ \ / _ \ '_ \  | |    |  __| | |  | | |  _  // _` |/ __/ _ \
 | |__| | |_) |  __/ | | | | |____| |____| |__| | | | \ \ (_| | (_|  __/
  \____/| .__/ \___|_| |_| |______|______|_____/  |_|  \_\__,_|\___\___|
        | |                                                             
        |_|          
 Open LED Race
 An minimalist cars race for LED strip  
  
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 by gbarbarov@singulardevices.com  for Arduino day Seville 2019 
 Code made dirty and fast, next improvements in: 
 https://github.com/gbarbarov/led-race
 https://www.hackster.io/gbarbarov/open-led-race-a0331a
 https://twitter.com/openledrace
*/

                                                            
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal.h>

#define MAXLED         300 // MAX LEDs actives on strip

#define PIN_LED        A0  // R 500 ohms to DI pin for WS2812 and WS2813, for WS2813 BI pin of first LED to GND  ,  CAP 1000 uF to VCC 5v/GND,power supplie 5V 2A  
#define PIN_P1         7   // switch player 1 to PIN and GND
#define PIN_P2         6   // switch player 2 to PIN and GND 
#define PIN_AUDIO      9   // through CAP 2uf to speaker 8 ohms


int NPIXELS = MAXLED; // leds on track

#define COLOR1    track.Color(0,255,0)
#define COLOR2    track.Color(255,0,0)

int win_music[] = {
  2637, 2637, 0, 2637, 
    0, 2093, 2637, 0,
    3136    
};

// configurando pinos do LCD
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
      
byte  gravity_map[MAXLED];

int milSec = 0;
int sec = 0;
int minutes = 0;

int flagMinutes = 0;
int flagHour = 0;

int TBEEP = 3; 

float speed1 = 0;
float speed2 = 0;
float dist1 = 0;
float dist2 = 0;

byte finished1 = 0;
byte finished2 = 0;

// contador de voltas dos players 1 e 2
byte loop1 = 0;
byte loop2 = 0;

// tempos dos players 1 e 2
unsigned long timeP1 = millis();
unsigned long timeP2 = millis();

// quem é o lider da corrida, player 1 ou 2
byte leader = 0;

// total de voltas da corrida
byte loop_max = 3;


float ACEL = 0.2;
float kf = 0.015; //friction constant
float kg = 0.003; //gravity constant

byte flag_sw1 = 0;
byte flag_sw2 = 0;
byte draworder = 0;
 
unsigned long timestamp=0;

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

int tdelay = 5; 

// escreve a estrutura do placar no LCD
void writeScoreBoardLCD(){

  // iniciando o LCD 16x2
  lcd.begin(16, 2);

  //Limpa a tela
  lcd.clear();

  // escrevendo a estrutura do placar (coluna, linha)

  // escrevendo players
  lcd.setCursor(0, 0);
  lcd.print("P");
  lcd.setCursor(1, 0);
  lcd.print("1");

  lcd.setCursor(0, 1);
  lcd.print("P");
  lcd.setCursor(1, 1);
  lcd.print("2");

  // voltas
  lcd.setCursor(3, 0);
  lcd.print("V");
  lcd.setCursor(3, 1);
  lcd.print("V");

  lcd.setCursor(5, 0);
  lcd.print(loop1);
  lcd.setCursor(5, 1);
  lcd.print(loop2);

  // tempo
  lcd.setCursor(8, 0);
  lcd.print("T");
  lcd.setCursor(8, 1);
  lcd.print("T");

  lcd.setCursor(10, 0);
  lcd.print("0:00.0");
  lcd.setCursor(10, 1);
  lcd.print("0:00.0");
}

// --- Rotina de Interrupção ---
ISR(TIMER1_OVF_vect){                              //interrupção do TIMER1 

  sec++;

  if (sec > 59){
    sec = 0;
    minutes++;
    
    if (minutes > 59){
      minutes = 0;
    }
  }

  TCNT1 = 0xC2F7;                                 // Renicia TIMER
}

// atualiza as voltas no placar
void updateTurn(int line, byte turn){
  
  lcd.setCursor(5, line);
  lcd.print(turn - 1);

}

void updateTime(int i, int n){

	for (int i = 0; i < n; i++){

		lcd.setCursor(10, i);
		lcd.print(minutes);

		lcd.setCursor(13, i);
  		lcd.print(":");
		
		if (sec < 10){
			lcd.setCursor(12, i);
			lcd.print("0");
			lcd.setCursor(13, i);
			lcd.print(sec); 
		}
		else{
			lcd.setCursor(12, i);
			lcd.print(sec); 
			
			lcd.setCursor(12, i);
			lcd.print(sec);
		}

		lcd.setCursor(14, i);
		lcd.print(".");

		milSec = millis() / 100;
		while (milSec > 10){
			milSec %= 10;
		}
		lcd.setCursor(15, i);
		lcd.print(milSec);
	}
}

// ----------------------------------------------------------------------------------
void set_ramp(byte H, byte a, byte b, byte c){

  // Gravidade de subida entre o começo e o topo da rampa
  for(int i = 0; i < (b - a); i++){
    gravity_map[a + i] = 127 - i * ((float)H / (b - a));
  };

  // Gravidade no topo da rampa
    gravity_map[b] = 127; 
  
  // Gravidade de decida entre o topo da rampa e o fim da rampa
    for(int i = 0; i < (c - b); i++){
    gravity_map[b + i + 1] = 127 + H - i * ((float)H / (c - b));
  };
}

// ----------------------------------------------------------------------------------
void set_loop(byte H,byte a,byte b,byte c){
    for(int i=0;i<(b-a);i++){
      gravity_map[a+i]=127-i*((float)H/(b-a));
    
  };
    
  gravity_map[b]=255; 

    for(int i=0;i<(c-b);i++){
    gravity_map[b+i+1]=127+H-i*((float)H/(c-b));
  };
}

// ----------------------------------------------------------------------------------
void setup() {
  // abre a porta serial a 9600 bps
  Serial.begin(9600);

  // Configuração do timer1 
  TCCR1A = 0;                        //confira timer para operação normal pinos OC1A e OC1B desconectados
  TCCR1B = 0;                        //limpa registrador
  TCCR1B |= (1<<CS10)|(1 << CS12);   // configura prescaler para 1024: CS12 = 1 e CS10 = 1
  
  TCNT1 = 0xC2F7;                    // incia timer com valor para que estouro ocorra em 1 segundo 65536-(16MHz/1024/1Hz) = 49911 = 0xC2F7
  
  TIMSK1 |= (1 << TOIE1);           // habilita a interrupção do TIMER1


  // iniciando a gravidade em todos os leds da pista
    for(int i = 0; i < NPIXELS; i++){
      gravity_map[i] = 127;
    };
    
  track.begin(); 
  // configurando os botões dos players 1 e 2
    pinMode(PIN_P1, INPUT_PULLUP); 
    pinMode(PIN_P2, INPUT_PULLUP);  

  // Pressionar o botão do player 1 para configurar ua rampa
    if ((digitalRead(PIN_P1) == 0)){
    // configurando uma rampa na pista, com o topo no 100 e com 10 leds de subida e 10 de subida
      set_ramp(12,90,100,110);
      
    // colocar a cor na rampa
    for(int i= 0; i < NPIXELS; i++){
      track.setPixelColor(i, track.Color(0, 0, (127 - gravity_map[i]) / 8));
    };

      track.show();
    };

  // escreve o placar no LCD
  writeScoreBoardLCD();
  // começa a corrida
    start_race();  

  sec = 0;

}

// ----------------------------------------------------------------------------------
void start_race(){
  for(int i = 0; i < NPIXELS; i++){
    track.setPixelColor(i, track.Color(0,0,0));
  };

  track.show();

  delay(2000);

  track.setPixelColor(12, track.Color(255,0,0));
  track.setPixelColor(11, track.Color(255,0,0));
  track.show();

  tone(PIN_AUDIO,400);

  delay(2000);

  noTone(PIN_AUDIO);

  track.setPixelColor(12, track.Color(0,0,0));
  track.setPixelColor(11, track.Color(0,0,0));
  track.setPixelColor(10, track.Color(255,255,0));
  track.setPixelColor(9, track.Color(255,255,0));
  track.show();

  tone(PIN_AUDIO,600);

  delay(2000);

  noTone(PIN_AUDIO);    

  track.setPixelColor(9, track.Color(0,0,0));
  track.setPixelColor(10, track.Color(0,0,0));
  track.setPixelColor(8, track.Color(0,255,0));
  track.setPixelColor(7, track.Color(0,255,0));
  track.show();

  tone(PIN_AUDIO,1200);

  delay(2000);

  noTone(PIN_AUDIO);     

  timestamp = 0;              
};

// ----------------------------------------------------------------------------------
void winner_fx(){
  int msize = sizeof(win_music) / sizeof(int);

  if (leader == 1){
    for(int i = 0; i < NPIXELS; i++){
      track.setPixelColor(i, COLOR1);
    }; 
  }
  else{
    for(int i = 0; i < NPIXELS; i++){
      track.setPixelColor(i, COLOR2);
    }; 
  }

  track.show();
  
  for (int note = 0; note < msize; note++) {
    tone(PIN_AUDIO, win_music[note], 200);
    delay(230);
    noTone(PIN_AUDIO);
  }                                             
};

// ----------------------------------------------------------------------------------
void draw_car1(void){
  for(int i = 0; i <= loop1; i++){
    track.setPixelColor( ((word)dist1 % NPIXELS) + i, track.Color(0, 255 - i * 20, 0) );
  };                   
}

// ----------------------------------------------------------------------------------
void draw_car2(void){
  for(int i = 0;i <= loop2; i++){
    track.setPixelColor(((word)dist2 % NPIXELS) + i, track.Color(255 - i * 20, 0, 0));
  };            
}
  
// ----------------------------------------------------------------------------------
void loop() {
    //for(int i=0;i<NPIXELS;i++){track.setPixelColor(i, track.Color(0,0,0));};

  updateTime(0, 2);
    for(int i = 0; i < NPIXELS; i++){
    track.setPixelColor(i, track.Color(0, 0, (127 - gravity_map[i]) / 8));
  };
    
    if ( (flag_sw1 == 1) && (digitalRead(PIN_P1) == 0) ) {
    flag_sw1 = 0;
    speed1 += ACEL;
  };

    if ( (flag_sw1 == 0) && (digitalRead(PIN_P1) == 1) ) {
    flag_sw1 = 1;
  };

    if ( (gravity_map[(word)dist1 % NPIXELS]) < 127 ){
    speed1 -= kg * (127 - (gravity_map[(word)dist1 % NPIXELS]));
  }; 
    
  if ( (gravity_map[(word)dist1 % NPIXELS]) > 127){
    speed1 += kg * ((gravity_map[(word)dist1 % NPIXELS]) - 127);
  };
    
    
  speed1 -= speed1 * kf; 
    
    if ( (flag_sw2 == 1) && (digitalRead(PIN_P2) == 0) ) {
    flag_sw2 = 0;
    speed2 += ACEL;
  };
    if ( (flag_sw2 == 0) && (digitalRead(PIN_P2) == 1) ) {
    flag_sw2 = 1;
  };

    if ( (gravity_map[(word)dist2 % NPIXELS]) < 127) {
    speed2 -= kg * (127 - (gravity_map[(word)dist2 % NPIXELS]));
  };

    if ( (gravity_map[(word)dist2 % NPIXELS]) > 127) {
    speed2 += kg * ((gravity_map[(word)dist2 % NPIXELS]) - 127);
  };

  speed2 -= speed2 * kf; 
      
  if (loop1 <= loop_max) dist1 += speed1;
    else finished1 = 1;

  if (loop2 <= loop_max) dist2 += speed2;
    else finished2 = 1;

  if (dist1 > dist2) {
    leader = 1;
  }; 

  if (dist2 > dist1) {
    leader = 2;
  };
      
  if (dist1 > NPIXELS * loop1) {
    loop1++;              // incrementando a volta do player 1
    
    updateTurn(0, loop1);       // atualizando as voltas no LCD

    tone(PIN_AUDIO, 600);
    TBEEP = 2;
  };

  if (dist2 > NPIXELS * loop2) {
    loop2++;              // incrementando a volta do player 2

    updateTurn(1, loop2);       // atualizando as voltas no LCD

    tone(PIN_AUDIO, 700);
    TBEEP = 2;
  };

  if (finished1 == 1 && finished2) == 1 {

    winner_fx();


    loop1 = 0;
    loop2 = 0;
    dist1 = 0;
    dist2 = 0;
    speed1 = 0;
    speed2 = 0;
    timestamp = 0;
    start_race();
  };

  if ((millis() & 512) == (512 * draworder)) {
    if (draworder==0) {
      draworder=1;
    }
    else {
      draworder=0;
    }   
  }; 

  if (draworder == 0) {
    if (loop1 <= loop_max) draw_car1();
    else finished1 = 1;
    if (loop2 <= loop_max) draw_car2();
    else finished2 = 1;
  }
  else {
    if (loop2 <= loop_max) draw_car2();
    else finished2 = 1;
    if (loop1 <= loop_max) draw_car1();
    else finished1 = 1;
  }   
                 
  track.show(); 
  delay(tdelay);
    
    if (TBEEP > 0) {
    TBEEP -= 1;
    // conflito de lib !!!! interrupção por neopixel
    if (TBEEP == 0) {
      noTone(PIN_AUDIO);
    }; 
  };   
}
