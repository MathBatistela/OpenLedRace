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

                                                            
#include <Adafruit_NeoPixel.h>		// biblioteca para controle de pixels e faixas de LED baseados em fio único
#include <LiquidCrystal.h>			// biblioteca para utilização do LCD
#include <EEPROM.h>					// escreve e faz leituras na EEPROM

#define MAXLED         300 			// Máximo de Led's ativos

#define PIN_LED        A0  			// R 500 ohms to DI pin for WS2812 and WS2813, for WS2813 BI pin of first LED to GND  ,  CAP 1000 uF to VCC 5v/GND,power supplie 5V 2A  
#define PIN_P1         7   			// pino do botão do player 1
#define PIN_P2         6   			// pino do botão do player 2
#define PIN_AUDIO      9   			// pino do buzzer
#define PIN_START      8   			// pino do botão para começar a corrida
 

int NPIXELS = MAXLED; 				// leds na pista

#define COLOR1    track.Color(0,255,0)			// cor do carro 1
#define COLOR2    track.Color(255,0,0)			// cor do carro 2

// notas tocadas, pelo buzzer, ao vencedor da corrida
int win_music[] = {
  2637, 2637, 0, 2637, 
    0, 2093, 2637, 0,
    3136    
};


LiquidCrystal lcd(12, 11, 5, 4, 3, 2);			// configurando pinos do LCD
      
byte  gravity_map[MAXLED];

byte loop_max = 2;								// total de voltas da corrida

int milSec = 0;									// milisegundos da corrida
int sec = 0;									// segundos da corrida
int minutes = 0;								// minutos da corrida

unsigned long timeWinner = 0;					// tempo do vencedor
unsigned long instantTime = 0;					// variavél que controlorá o máximo de tempo que o segundo colocado poderá chegar após o vencedor

int TBEEP = 3; 

float speed1 = 0;								// velocidade player 1
float speed2 = 0;								// velocidade player 2
float dist1 = 0;								// distância percorrida pelo player 1
float dist2 = 0;								// distância percorrida pelo player 2


String buffer = "";								// buffer para a leitura da porta serial
boolean newData = false;						// controla quando há uma nova palavra no buffer para ser processado ou não.


// variaveis que controlam se um player terminou a corrida ou não
byte finished1 = 0;
byte finished2 = 0;

// contador de voltas dos players 1 e 2
byte loop1 = 0;
byte loop2 = 0;

// quem é o vencedor da corrida, player 1 ou 2
byte winner = 0;

// variavel que controla o inicio da corrida
byte start_flag = 0;


float ACEL = 0.2;
float kf = 0.015; 								// constante de atrito
float kg = 0.003; 								// constante de gravidade

byte flag_sw1 = 0;
byte flag_sw2 = 0;
byte draworder = 0;
 
unsigned long timestamp = 0;

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

int tdelay = 5; 

// ----------------- Protótipo das Funções -----------------

void clearRank();									// limpa o rank da EEPROM
void listRank();									// lista o rank da EEPROM

void checkSerialPort();								// checa se tem caracter na porta serial para serem processados e toma a decisão que for necessária

void writeScoreBoardLCD();							// escreve a estrutura do placar no LCD
void updateTurn(int line, byte turn);				// atualiza as voltas no placar; line = linha do LCD (0 ou 1); turn = número da volta;
void updateTime(int i);			        			// atualiza o tempo da corrida no placar; i = linha do LCD(0 ou 1) ;

void set_ramp(byte H, byte a, byte b, byte c);		// cria uma rampa no circuito
void set_loop(byte H,byte a,byte b,byte c);		   	// cria um loop no circuito

void start_race();									// rotina para começar uma corrida
void winner_fx();							  		// rotina do vencedor da corrida

void draw_car1();							 		// rotina de desenho do carro do player 1
void draw_car2();							  		// rotina de desenho do carro do player 2

void finish_race();									// rotina de finalização de uma corrida

void printInfoWinner(int player, int record);		// mostra na porta serial as informações do vencedor da corrida; player = jogador vencedor; record = se o jogador bateu um recorde e se sim, qual a posição;


// ----------------- Rotina de Interrupção -----------------

// interrupção do TIMER1 a cada 1 segundo
ISR(TIMER1_OVF_vect){                               

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


// ------------------- Código das Funções ------------------

void clearRank(){
	char nome[7] = {'N', 'e', 'n', 'h', 'u', 'm', '\0'};
	char tempo[7] = {'0', '0', ':', '0', '0', '.', '0'};
	char data[10] = {'0', '0', '/', '0', '0', '/', '0', '0', '0', '0'};
  
	for (int i = 0; i < 10; i++){
		for (int j = 0; j < 7; j++){
			EEPROM.write(((50 * i) + j), nome[j]);
			EEPROM.write((((50 * i) + 33) + j), tempo[j]);
		}

		for (int j = 0; j < 10; j++){
			EEPROM.write((((50 * i) + 40) + j), data[j]);
		}
	
	}
  
  	Serial.print("Ranks Resetados!\n");
}

// ---------------------------------------------------------
void listRank(){

	Serial.print("\n\tNome\t\t\t\t\tTempo\t\tData\n\n");
	char aux = "";
  	for (int i = 0; i < 10; i++){
		Serial.print(i+1); Serial.print("\t"); 
		int k = 0;
		
		while (EEPROM.read((50 * i) + k) != '\0'){
			aux = EEPROM.read((50 * i) + k);
			Serial.print(aux);
			k++;
		} 
		
		Serial.print("\t\t\t\t\t");
		for (int j = 0; j < 7; j++){
			aux = EEPROM.read(((50 * i) + 33) + j);
			Serial.print(aux);  
		}

    	Serial.print("\t\t");
		for (int j = 0; j < 10; j++){
			aux = EEPROM.read(((50 * i) + 40) + j);
			Serial.print(aux);  
		}

    	Serial.print("\n"); 
  	}
  
  	Serial.print("\n");
}

// ---------------------------------------------------------
void checkSerialPort(){
	char endMarker = '\n';						// caracter que delimitara o fim de uma palavra
    char rc;									// caracter auxiliar para que seja concatenado no buffer
   
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();						// lê caracter por caracter que foi enviado pelo porta serial

        if (rc != endMarker) 
			buffer.concat(rc);					// concatena as letras do rc para formar uma string
        else 
			newData = true; 					// sinaliza que tem uma nova palavra para ser processada
    }


  	if (newData == true) {
      	Serial.print(buffer); Serial.print("\n"); 
        
		// tomar decisão de acordo com o que estiver no buffer
		if (buffer.equals("clearRank")){
          	clearRank();
      	}
		else if (buffer.equals("listRank")){
			listRank();
		}
    	else{
      		Serial.print("Error! Nenuma opção válida foi digitada.\n");
    	}

    	buffer = "";			// limpa o buffer
        newData = false;		// sinaliza que a palavra já foi processada
    }
}

// ---------------------------------------------------------
void writeScoreBoardLCD(){

	lcd.clear();				// limpando LCD

	// escrevendo a estrutura do placar (coluna, linha)
    for (int i = 0; i < 2; i++){
      	// escrevendo players
		lcd.setCursor(0, i);
		lcd.print("P");
		lcd.setCursor(1, i);
		lcd.print(i + 1);

		// voltas
		lcd.setCursor(3, i);
		lcd.print("V");

		lcd.setCursor(5, i);
		lcd.print(0);

		// tempo
		lcd.setCursor(8, i);
		lcd.print("T");

		lcd.setCursor(10, i);
		lcd.print("0:00.0");

    }
}

// ---------------------------------------------------------
void updateTurn(int line, byte turn){
  
	lcd.setCursor(5, line);
	lcd.print(turn - 1);

}

// ---------------------------------------------------------
void updateTime(int i){

	// se algum player já tiver terminado a corrida não atualiza seu tempo
	if (i == 0 && finished1 == 1) return;    
	if (i == 1 && finished2 == 1) return;
    

    lcd.setCursor(10, i);
    lcd.print(minutes);

    lcd.setCursor(11, i);
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

// ---------------------------------------------------------
void set_ramp(byte H, byte a, byte b, byte c){

	// Gravidade de subida entre o começo e o topo da rampa
	for(int i = 0; i < (b - a); i++){
		gravity_map[a + i] = 127 - i * ( (float)H / (b - a));
	};

	// Gravidade no topo da rampa
	gravity_map[b] = 127; 
  
	// Gravidade de decida entre o topo da rampa e o fim da rampa
	for(int i = 0; i < (c - b); i++){
		gravity_map[b + i + 1] = 127 + H - i * ((float)H / (c - b));
	};
}

// ---------------------------------------------------------
void set_loop(byte H,byte a,byte b,byte c){
    
	// Gravidade de subida entre o começo e o topo do loop
	for(int i = 0; i < (b - a); i++){
      	gravity_map[a + i]= 127 - i * ( (float)H / (b - a));
  	};
    
	// Gravidade no topo do loop
  	gravity_map[b] = 255; 

	// Gravidade de decida entre o topo do loop e o fim do loop
    for(int i = 0; i < (c - b); i++){
    	gravity_map[b + i + 1] = 127 + H - i * ( (float)H / (c - b));
  	};
}

// ---------------------------------------------------------
void start_race(){
  
	writeScoreBoardLCD();			// escreve o placar no LCD

	// começa o semáfaro do jogo
	track.show();
	delay(2000);

	// cor vermelha
	track.setPixelColor(12, track.Color(255,0,0));
	track.setPixelColor(11, track.Color(255,0,0));
	track.show();

	tone(PIN_AUDIO,400);
	delay(2000);
	noTone(PIN_AUDIO);

	// cor amarela
	track.setPixelColor(12, track.Color(0,0,0));
	track.setPixelColor(11, track.Color(0,0,0));
	track.setPixelColor(10, track.Color(255,255,0));
	track.setPixelColor(9, track.Color(255,255,0));
	track.show();

	tone(PIN_AUDIO,600);
	delay(2000);
	noTone(PIN_AUDIO);    
	
	// cor verde
	track.setPixelColor(9, track.Color(0,0,0));
	track.setPixelColor(10, track.Color(0,0,0));
	track.setPixelColor(8, track.Color(0,255,0));
	track.setPixelColor(7, track.Color(0,255,0));
	track.show();

	tone(PIN_AUDIO,1200);
	delay(2000);
	noTone(PIN_AUDIO);     

	timestamp = 0;              
	
	milSec = 0;
	sec = 0;
	minutes = 0;

	start_flag = 1;

};

// ---------------------------------------------------------
void winner_fx(){
	int msize = sizeof(win_music) / sizeof(int);

	// coloca no LCD o player vencedor
	lcd.clear();
	lcd.setCursor(5, 0);
	lcd.print("Winner");
	lcd.setCursor(4, 1);
	lcd.print("Player");
	lcd.setCursor(11, 1);
	lcd.print(winner);


	// acende toda a pista da cor do vencedor
	if (winner == 1){
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
	
	// toca a música do vencedor
	for (int note = 0; note < msize; note++) {
		tone(PIN_AUDIO, win_music[note], 200);
		delay(230);
		noTone(PIN_AUDIO);
	}

	delay(3000);
	lcd.clear();                                             
};

// ---------------------------------------------------------
void draw_car1(){
	for(int i = 0; i <= loop1; i++){
		track.setPixelColor( ((word)dist1 % NPIXELS) + i, track.Color(0, 255 - i * 20, 0) );
	};                   
}

// ---------------------------------------------------------
void draw_car2(){
	for(int i = 0;i <= loop2; i++){
		track.setPixelColor(((word)dist2 % NPIXELS) + i, track.Color(255 - i * 20, 0, 0));
	};            
}

// ---------------------------------------------------------
void finish_race(){
	loop1 = 0;
	loop2 = 0;
	dist1 = 0;
	dist2 = 0;
	speed1 = 0;
	speed2 = 0;
	timestamp = 0;
	finished1 = 0;
	finished2 = 0;
	timeWinner = 0;
	winner = 0;
	instantTime = 0;
	start_flag = 0;

	for(int i = 0; i < NPIXELS; i++){
		track.setPixelColor(i, track.Color(0, 0, 0));
	};

}

// ---------------------------------------------------------
void printInfoWinner(int player, int record){
	Serial.print("Vencedor: Player "); Serial.print(player);
	if (record > 0 && record < 11){
		Serial.print("\tRecorde batido! Posição: "); Serial.print(record);
	}
	Serial.print("\n");
	
	Serial.print("Tempo do vencedor: "); Serial.print(timeWinner);  Serial.print("\t");
	Serial.print(timeWinner / 60000); Serial.print(":"); 									// minutos
	Serial.print((timeWinner % 60000) / 1000); Serial.print(".");							// segundos
	Serial.print(((timeWinner % 60000) % 1000) / 100); Serial.print("\n\n");				// milisegundos
}


// --------------------- Inicialização ---------------------
void setup() {
    
	Serial.begin(9600);				// abre a porta serial a 9600 bps
    lcd.begin(16, 2);				// inicia o LCD 16x2

	// Configurações do TIMER1 
	TCCR1A = 0;                        // confira timer para operação normal pinos OC1A e OC1B desconectados
	TCCR1B = 0;                        // limpa registrador
	TCCR1B |= (1<<CS10)|(1 << CS12);   // configura prescaler para 1024: CS12 = 1 e CS10 = 1
	
	TCNT1 = 0xC2F7;                    // incia timer com valor para que estouro ocorra em 1 segundo 65536-(16MHz/1024/1Hz) = 49911 = 0xC2F7
	
	TIMSK1 |= (1 << TOIE1);           // habilita a interrupção do TIMER1


  	// iniciando a gravidade em todos os leds da pista
    for(int i = 0; i < NPIXELS; i++){
		gravity_map[i] = 127;
    };
    
  	track.begin(); 
  	
	// configurando os botões dos players 1 e 2 e o de começo de corrida
    pinMode(PIN_P1, INPUT_PULLUP); 
    pinMode(PIN_P2, INPUT_PULLUP);  
    pinMode(PIN_START, INPUT_PULLUP);

  	// Pressionar o botão do player 1 para configurar uma rampa no circuito
    if ((digitalRead(PIN_P1) == 0)){
    	// configurando uma rampa na pista, com o topo no 100 e com 10 leds de subida e 10 de subida
     	 set_ramp(12,90,100,110);
      
		// colocar a cor na rampa de acordo com a gravidade
		for(int i= 0; i < NPIXELS; i++){
			track.setPixelColor(i, track.Color(0, 0, (127 - gravity_map[i]) / 8));
		};

      	track.show();
    };

}


// -------------------- Laço Principal ---------------------
void loop() {

  	checkSerialPort();

	// define o início da corrida quando o botão start for pressionado
	if(digitalRead(PIN_START) == 0){
		if(start_flag == 0)
			start_race();
		else{
			finish_race();
			delay(500);
		}
	}

  	if(start_flag){

		// atualizando tesmpos de todos os players
		for (int i = 0; i < 2; i++){
			updateTime(i);
		}

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
			

		// faz a checagem se o player 1 terminou a corrida ou não
		if (loop1 <= loop_max) 
			dist1 += speed1;
		else{
			updateTime(0);
			finished1 = 1;
		} 

		// faz a checagem se o player 2 terminou a corrida ou não
		if (loop2 <= loop_max)
			dist2 += speed2;
		else{
			updateTime(1);
			finished2 = 1;
		} 
			
		if (dist1 > NPIXELS * loop1) {
			loop1++;              		// incrementando a volta do player 1
			
			updateTurn(0, loop1);       // atualizando as voltas no LCD

			tone(PIN_AUDIO, 600);		// toca o buzzer avisando que uma volta foi concluída
			TBEEP = 2;
		};

		if (dist2 > NPIXELS * loop2) {
			loop2++;              		// incrementando a volta do player 2

			updateTurn(1, loop2);       // atualizando as voltas no LCD

			tone(PIN_AUDIO, 700);		// toca o buzzer avisando que uma volta foi concluída
			TBEEP = 2;
		};


		if(timeWinner){
			instantTime = (sec * 1000) + (milSec * 100);
			instantTime = (instantTime / 1000);
		}

		// termina a corrida se os dois concluiram todas as voltas, ou passou-se de 10 segundos que o vencedor concluiu a corrida 
		if ((finished1 && finished2) || ((instantTime - ((timeWinner % 60000) / 1000)) >= 10)) {
			winner_fx();
			finish_race();
		};

		if ((millis() & 512) == (512 * draworder)) {
			if (draworder==0) {
				draworder=1;
			}
			else {
				draworder=0;
			}   
		} 

		if (draworder == 0) {
			if (!finished1) draw_car1();
			if (!finished2) draw_car2();
		}
		else {
			if (!finished2) draw_car2();
			if (!finished1) draw_car1();
		}

		if(finished1){
			if (winner == 0){
				timeWinner = (minutes * 60000) + (sec * 1000) + (milSec * 100);
				printInfoWinner(1, 0);
			}
			winner = 1;

			for(int i = 0; i < 3; i++){
				track.setPixelColor(i, COLOR1);
			}; 
		}
		else if(finished2){
			if (winner == 0){
				timeWinner = (minutes * 60000) + (sec * 1000) + (milSec * 100);
				printInfoWinner(2, 0);
				
				// Serial.print("Tempo vencedor: "); Serial.print(timeWinner);  Serial.print("\n");
				// Serial.print("Minutos: "); Serial.print(timeWinner / 60000); Serial.print("\n");
				// Serial.print("Segundos: "); Serial.print((timeWinner % 60000) / 1000); Serial.print("\n");
				// Serial.print("Ms: "); Serial.print(((timeWinner % 60000) % 1000) / 100); Serial.print("\n");
			}
			winner = 2;
			
			for(int i = 0; i < 3; i++){
				track.setPixelColor(i, COLOR2);
			}; 
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
	else{
		lcd.setCursor(5, 0);
		lcd.print("Press");
		lcd.setCursor(4, 1);
		lcd.print("To start");        
	}

}
