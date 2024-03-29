// Este código pode ser obtido no GitHub pelo link: https://github.com/MathBatistela/OpenLedRace

// * ------------------------------------------------------------------ *
// * NOME DO ARQUIVO :    led_race.ino                                  *
// *                                                                    *
// * DESCRIÇÂO :                                                        *
// *                                                                    *
// *        Este trabalho consiste em uma corrida LED de dois           *
// *          jogadores em uma pista feita de fita LED endereçavel      *
// *          e o jogador que terminar a corrida no menor tempo ganha   *
// *          a corrida, e se este terminar em um tempo menor do que    *
// *          está na EEPROM este jogador pode escrever seu nome        *
// *          na posição do seu respectivo recorde.                     *
// *                                                                    *
// *                                                                    *
// *                                                                    *
// *                                                                    *
// * AUTORES :    Enzo Italiano, Henrique Marcuzzo e Matheus Batistela  *
// *                                                                    *
// * DATA DE CRIAÇÃO :    15/11/2019                                    *   
// *                                                                    *
// * MODIFICAÇÕES :       04/11/2019                                    *
// *                                                                    * 
// **********************************************************************

#include <Adafruit_NeoPixel.h>              // biblioteca para controle de pixels e faixas de LED baseados em fio único
#include <LiquidCrystal.h>                  // biblioteca para utilização do LCD
#include <EEPROM.h>                         // escreve e faz leituras na EEPROM
#include <string.h>                         // manipualação de strings
#include <ctype.h>
#include <DS3231.h>                         // biblioteca para utilização do relógio de tempo real

#define MAXLED         300      // Máximo de Led's ativos

#define PIN_LED        A0       // R 500 ohms to DI pin for WS2812 and WS2813, for WS2813 BI pin of first LED to GND  ,  CAP 1000 uF to VCC 5v/GND,power supplie 5V 2A  
#define PIN_P1         7        // pino do botão do player 1
#define PIN_P2         6        // pino do botão do player 2
#define PIN_AUDIO      9        // pino do buzzer
#define PIN_START      8        // pino do botão para começar a corrida
 

int NPIXELS = MAXLED;           // leds na pista 

#define COLOR1    track.Color(255,0,0)        // cor do carro 1
#define COLOR2    track.Color(0,255,0)        // cor do carro 2

// notas tocadas, pelo buzzer, ao vencedor da corrida
int win_music[] = {
  2637, 2637, 0, 2637, 
    0, 2093, 2637, 0,
    3136    
};


LiquidCrystal lcd(12, 11, 5, 4, 3, 2);      // configurando pinos do LCD
      
byte  gravity_map[MAXLED];

byte loop_max = 3;                          // total de voltas da corrida

int milSec = 0;                             // milisegundos da corrida
int sec = 0;                                // segundos da corrida
int minutes = 0;                            // minutos da corrida

byte clear = 0;

unsigned long timeWinner = 0;               // tempo do vencedor
unsigned long instantTime = 0;              // variavél que controlorá o máximo de tempo que o segundo colocado poderá chegar após o vencedor

int TBEEP = 3; 

float speed1 = 0;                           // velocidade player 1
float speed2 = 0;                           // velocidade player 2
float dist1 = 0;                            // distância percorrida pelo player 1
float dist2 = 0;                         // distância percorrida pelo player 2


String buffer = "";                   // buffer para a leitura da porta serial
boolean newData = false;                // controla quando há uma nova palavra no buffer para ser processado ou não.


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


float ACEL = 0.2;             // constante de aceleração
float kf = 0.015;                       // constante de atrito
float kg = 0.003;                       // constante de gravidade

byte flag_sw1 = 0;
byte flag_sw2 = 0;
byte draworder = 0;

int tdelay = 5; 

char auxChar[2];              // auxiliar de caracteres
int auxInt;                 // auxiliar de inteiros

int i, j;

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);
DS3231 rtc(SDA, SCL);


// ----------------- Protótipo das Funções -----------------

void clearRank();                                   // limpa o rank da EEPROM
void listRank();                                    // lista o rank da EEPROM
void writeName();                                   // escreve seu nome em uma posição da EEPROM

void checkSerialPort();                               // checa se tem caracter na porta serial para serem processados e toma a decisão que for necessária

void writeScoreBoardLCD();                            // escreve a estrutura do placar no LCD
void updateTurn(int line, byte turn);                   // atualiza as voltas no placar; line = linha do LCD (0 ou 1); turn = número da volta;
void updateTime(int i);                                 // atualiza o tempo da corrida no placar; i = linha do LCD(0 ou 1) ;

void set_ramp(byte H, byte a, byte b, byte c);              // cria uma rampa no circuito
void set_loop(byte H, byte a, byte b, byte c);              // cria um loop no circuito

void start_race();                                  // rotina para começar uma corrida
void winner_fx();                                   // rotina do vencedor da corrida
void record_fx(int winner, int strobeCount, int flashDelay);    // pisca o Led todo com a cor do vencedor caso ele tenha batido um recorde

void draw_car1();                                   // rotina de desenho do carro do player 1
void draw_car2();                                   // rotina de desenho do carro do player 2

void finish_race();                                 // rotina de finalização de uma corrida

void printInfoWinner(int player, int record);             // mostra na porta serial as informações do vencedor da corrida; player = jogador vencedor; record = se o jogador bateu um recorde e se sim, qual a posição;

void fillRecordStruct(int pos);                         // prenche a estruta de records para que possa ser utilizada para checar novos records   
int checkRecord();                                // checa se o vencedor bateu algum recorde, se bateu é escrito o recorde na posição correta da struct e na EEPROM; O retorno deve ser a poição do recorde se houver, ou 0 casa o não haja recorde.   
void writeRecord(int Actualrecord);                   // escreve na posição do recorde, os dados do recorde na struct; actualRecord = posição que o novo dado entrará na struct 

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
  
  for (i = 0; i < 10; i++){
    for (j = 0; j < 7; j++){
      EEPROM.write(((50 * i) + j), nome[j]);
      EEPROM.write((((50 * i) + 33) + j), tempo[j]);
    }

    for (j = 0; j < 10; j++){
      EEPROM.write((((50 * i) + 40) + j), data[j]);
    }

  }

  Serial.print(F("Ranks Resetados!\n"));
}

// ---------------------------------------------------------
void listRank(){

  Serial.print(F("\n\tNome\t\t\t\t\tTempo\t\tData\n\n"));


  for (i = 0; i < 10; i++){
    Serial.print(i+1); Serial.print(F("\t")); 
    int k = 0;

    while (EEPROM.read((50 * i) + k) != '\0'){
      auxChar[0] = EEPROM.read((50 * i) + k);
      Serial.print(auxChar[0]);
      k++;
    } 

    Serial.print(F("\t\t\t\t\t"));
    for (j = 0; j < 7; j++){
      auxChar[0] = EEPROM.read(((50 * i) + 33) + j);
      Serial.print(auxChar[0]);  
    }

    Serial.print(F("\t\t"));
    for (j = 0; j < 10; j++){
      auxChar[0] = EEPROM.read(((50 * i) + 40) + j);
      Serial.print(auxChar[0]);  
    }

    Serial.print(F("\n")); 
  }

  Serial.print(F("\n"));
}

// ---------------------------------------------------------
void writeName(){
    
  int auxI;
  char auxC;

  i = 0;
  newData = false;
  
  // limpando auxiliar de caracteres
  auxChar[0] = '\0';
  auxChar[1] = '\0';

  Serial.print(F("Digite a posição do rank: "));

  while (newData == false){
    while (Serial.available() > 0){
      auxC = Serial.read();               // lê caracter por caracter que foi enviado pelo porta serial
      if (auxC != '\n' && i < 2){
        auxChar[i] = auxC;              // adiciona no auxiliar de caracters um máximo de duas letras
        i++;
      }
      else
        newData = true;               // sinaliza que tem uma nova palavra para ser processada

    }
  }


  if (newData == true){
    
    auxI = atoi(auxChar);           // transforma o auxiliar de caracteres em um número inteiro   
    Serial.print(auxI); Serial.print(F("\n"));

    // checa se o número digitado está dentro do range válido
    if (auxI > 10 || auxI < 1){
      Serial.print(F("Posição inválida! Somente posições de 1 à 10 são aceitas!\n"));

      // limpa eventuais lixos na porta serial
      while (Serial.available() > 0){
        auxChar[0] = Serial.read();
      }
    }
    else{
      fillRecordStruct(auxI - 1);
      // checa se a posição digitada existe alguma corrida, caso contrário não é um rank válido
      if (instantTime == 0){
        Serial.print(F("Posição inválida! Não há nenhum recorde nesta posição!\n")); 

        // limpa eventuais lixos na porta serial
        while (Serial.available() > 0){
          auxChar[0] = Serial.read();
        }
      }
      else{
        i = 0;
        j = 1;

        // limpa eventuais lixos na porta serial
        while (Serial.available() > 0){
          auxChar[0] = Serial.read();
        }

        Serial.print(F("Digite o seu nome (máximo de 32 caracteres): "));
        
        while(j){
          while (Serial.available() > 0){
            auxChar[0] = Serial.read();
            
            if (auxChar[0] != '\n' && i < 33){
              Serial.print(auxChar[0]);
              EEPROM.write(((50 * (auxI - 1)) + i), auxChar[0]);
              i++;
            }
            else
              j = 0;
          }
        }

        EEPROM.write(((50 * (auxI - 1)) + i), '\0');
        Serial.print(F("\n\nNome Escrito com sucesso!\n"));
      }
    }
  }
}

// ---------------------------------------------------------
void checkSerialPort(){

  while (Serial.available() > 0 && newData == false) {
    auxChar[0] = Serial.read();               // lê caracter por caracter que foi enviado pelo porta serial

    if (auxChar[0] != '\n')
      buffer.concat(auxChar[0]);            // concatena as letras do rc para formar uma string
    else
      newData = true;                   // sinaliza que tem uma nova palavra para ser processada
  }


  if (newData == true) {
    Serial.print(buffer); Serial.print(F("\n")); 
    
    // tomar decisão de acordo com o que estiver no buffer
    if (buffer.equals("clearRank"))
        clearRank();
    else if (buffer.equals("listRank"))
      listRank();
    else if (buffer.equals("writeName"))
      writeName();
    else{
      Serial.print(F("\nError! Nenuma opção válida foi digitada.\n"));
      Serial.print(F("As opções são:\n\n"));
      Serial.print(F("\"clearRank\" - para limpar todas as posições do rank\n"));
      Serial.print(F("\"listRank\" - para listar os 10 corredores que terminaram mais rápido a corrida\n"));
      Serial.print(F("\"writeName\" - para escrever seu nome em uma posição caso queria registrar sua vitória! =D\n\n"));
    }

    buffer = "";      // limpa o buffer
    newData = false;    // sinaliza que a palavra já foi processada
  }
}

// ---------------------------------------------------------
void writeScoreBoardLCD(){

  lcd.clear();              // limpando LCD
  clear = 0;

  // escrevendo a estrutura do placar (coluna, linha)
  for (i = 0; i < 2; i++){
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
  for(i = 0; i < (b - a); i++){
    gravity_map[a + i] = 127 - i * ( (float)H / (b - a));
  };

  // Gravidade no topo da rampa
  gravity_map[b] = 127; 

  // Gravidade de decida entre o topo da rampa e o fim da rampa
  for(i = 0; i < (c - b); i++){
    gravity_map[b + i + 1] = 127 + H - i * ((float)H / (c - b));
  };
}

// ---------------------------------------------------------
void set_loop(byte H,byte a,byte b,byte c){
    
  // Gravidade de subida entre o começo e o topo do loop
  for(i = 0; i < (b - a); i++){
    gravity_map[a + i]= 127 - i * ( (float)H / (b - a));
  };

  // Gravidade no topo do loop
  gravity_map[b] = 255; 

  // Gravidade de decida entre o topo do loop e o fim do loop
  for(i = 0; i < (c - b); i++){
    gravity_map[b + i + 1] = 127 + H - i * ( (float)H / (c - b));
  };
}

// ---------------------------------------------------------
void start_race(){
  
  writeScoreBoardLCD();     // escreve o placar no LCD

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

  milSec = 0;
  sec = 0;
  minutes = 0;

  start_flag = 1;

};

// ---------------------------------------------------------
void winner_fx(){
  int msize = sizeof(win_music) / sizeof(int);
  int actualRecord = -1;

  // coloca no LCD o player vencedor
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Winner");
  lcd.setCursor(4, 1);
  lcd.print("Player");
  lcd.setCursor(11, 1);
  lcd.print(winner);

  actualRecord = checkRecord();         // checa se algum recorde foi batido


  printInfoWinner(winner, (actualRecord + 1));  // se o recorde for batido será mostrando na porta serial a posição e o aviso


  // acende toda a pista da cor do vencedor
  if (winner == 1){
    for(i = 0; i < NPIXELS; i++){
      track.setPixelColor(i, COLOR1);
    }; 
  }
  else{
    for(i = 0; i < NPIXELS; i++){
      track.setPixelColor(i, COLOR2);
    }; 
  }
  track.show();

  // toca a música do vencedor
  for (i = 0; i < msize; i++) {
    tone(PIN_AUDIO, win_music[i], 200);
    delay(230);
    noTone(PIN_AUDIO);
  }

  // se recorde foi batido o Led terá um efeito piscante avisando, visualmente, da conquista
  if (actualRecord != -1)
    record_fx(winner, 20, 100);

  delay(3000);
  lcd.clear();                                             
};

// ---------------------------------------------------------
void record_fx(int winner, int strobeCount, int flashDelay){

  for(j = 0; j < strobeCount; j++) {
    if(winner == 1){
      for(i = 0; i < NPIXELS; i++){
        track.setPixelColor(i, COLOR1);
      }
    }
    else{
      for(i = 0; i < NPIXELS; i++){
        track.setPixelColor(i, COLOR2);
      }
    }    
    track.show();

    delay(flashDelay);

    for(i = 0; i < NPIXELS; i++){
      track.setPixelColor(i, track.Color(0,0,0));
    }
    track.show();

    delay(flashDelay);
  }

}

// ---------------------------------------------------------
void draw_car1(){
  for(i = 0; i <= loop1; i++){
    track.setPixelColor( ((word)dist1 % NPIXELS) + i, COLOR1);
  };                   
}

// ---------------------------------------------------------
void draw_car2(){
  for(i = 0;i <= loop2; i++){
    track.setPixelColor(((word)dist2 % NPIXELS) + i, COLOR2);
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
  finished1 = 0;
  finished2 = 0;
  timeWinner = 0;
  winner = 0;
  instantTime = 0;
  start_flag = 0;

  for(i = 0; i < NPIXELS; i++){
    track.setPixelColor(i, track.Color(0, 0, 0));
  };

}

// ---------------------------------------------------------
void printInfoWinner(int player, int record){
  Serial.print(F("Vencedor: Player ")); Serial.print(player);
  if (record > 0 && record < 11)
    Serial.print(F("\tRecorde batido! Posição: ")); Serial.print(record);

  Serial.print("\n");

  Serial.print(F("Tempo do vencedor: ")); Serial.print(timeWinner);  Serial.print(F("\t"));
  Serial.print(timeWinner / 60000); Serial.print(F(":"));                       // minutos
  Serial.print((timeWinner % 60000) / 1000); Serial.print(F("."));                // segundos
  Serial.print(((timeWinner % 60000) % 1000) / 100); Serial.print(F("\n\n"));        // milisegundos
}

// ---------------------------------------------------------
void fillRecordStruct(int pos){

  instantTime = 0; 

  for (j = 0; j < 7; j++){
    auxChar[0] = EEPROM.read(((50 * pos) + 33) + j);

    if (j != 2 && j != 5)
      auxInt = auxChar[0] - '0';
    

    if (j == 0)
      instantTime = instantTime + (auxInt * 600000);
    if (j == 1)
      instantTime = instantTime + (auxInt * 60000);
    if (j == 3)
      instantTime = instantTime + (auxInt * 10000);
    if (j == 4)
      instantTime = instantTime + (auxInt * 1000);
    if (j == 6)
      instantTime = instantTime + (auxInt * 100); 

  }  
}

// ---------------------------------------------------------
int checkRecord(){

  int actualRecord = -1;

  for (i = 0; i < 10; i++){
    fillRecordStruct(i);
    if (instantTime == 0 || instantTime > timeWinner){
      actualRecord = i; 
      break;
    }
  } 


  if (actualRecord != -1){
    for (i = 9; i > actualRecord; i--){
      j = 0;

      while (EEPROM.read((50 * (i - 1)) + j) != '\0'){
        auxChar[0] = EEPROM.read((50 * (i - 1)) + j);
        EEPROM.write((50 * i) + j, auxChar[0]);
        j++;
      } 
      auxChar[0] = '\0';
      EEPROM.write((50 * i) + j, auxChar[0]);
      
      for (j = 0; j < 7; j++){
        auxChar[0] = EEPROM.read(((50 * (i - 1)) + 33) + j);
        EEPROM.write(((50 * i ) + 33) + j, auxChar[0]);
      }

      for (j = 0; j < 10; j++){
        auxChar[0] = EEPROM.read(((50 * (i - 1)) + 40) + j);
        EEPROM.write(((50 * i) + 40) + j, auxChar[0]);
      }
    }

    writeRecord(actualRecord);
  }

  return actualRecord;
}

// ---------------------------------------------------------
void writeRecord(int actualRecord){
  char newNome[13] = {'D', 'e', 's', 'c', 'o', 'n', 'h', 'e', 'c', 'i', 'd', 'o', '\0'};

  // escrecendo na posição do recorde batido os novos valores
  for (i = 0; i < 13; i++){
    EEPROM.write((50 * actualRecord) + i, newNome[i]);
  }

  for (i = 0; i < 7; i++){
    if (i == 0)
      auxChar[0] = (timeWinner / 600000) + '0';
    else if (i == 1)
      auxChar[0] = ((timeWinner % 600000) / 60000) + '0';
    else if (i == 2)
      auxChar[0] = ':';
    else if (i == 3)
      auxChar[0] = (((timeWinner % 600000) % 60000) / 10000) + '0';
    else if (i == 4)
      auxChar[0] = ((((timeWinner % 600000) % 60000) % 10000) / 1000) + '0';
    else if (i == 5)
      auxChar[0] = '.';
    else if (i == 6)
      auxChar[0] = (((((timeWinner % 600000) % 60000) % 10000) % 1000) / 100) + '0';

    EEPROM.write(((50 * actualRecord) + 33) + i, auxChar[0]);
  }

  for (i = 0; i < 10; i++){
    auxChar[0] = rtc.getDateStr()[i];

    if (i == 2 || i == 5)
      auxChar[0] = '/';

    EEPROM.write(((50 * actualRecord) + 40) + i, auxChar[0]);
  }
  
}


// --------------------- Inicialização ---------------------
void setup() {
    
  Serial.begin(9600);             // abre a porta serial a 9600 bps
  lcd.begin(16, 2);             // inicia o LCD 16x2
  rtc.begin();                // aciona o relógio


//   rtc.setDate(4, 12, 2019); // setar a data do módulo


  // Configurações do TIMER1 
  TCCR1A = 0;                         // confira timer para operação normal pinos OC1A e OC1B desconectados
  TCCR1B = 0;                         // limpa registrador
  TCCR1B |= (1<<CS10)|(1 << CS12);    // configura prescaler para 1024: CS12 = 1 e CS10 = 1

  TCNT1 = 0xC2F7;                     // incia timer com valor para que estouro ocorra em 1 segundo 65536-(16MHz/1024/1Hz) = 49911 = 0xC2F7

  TIMSK1 |= (1 << TOIE1);         // habilita a interrupção do TIMER1


  // iniciando a gravidade em todos os leds da pista
  for(i = 0; i < NPIXELS; i++){
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
    set_ramp(22,70,100,130);


    // configurando um loop na pista, com o topo no 200 e com 10 leds de subida e 10 de subida
    set_loop(32,170,200,230);
    
    // colocar a cor na rampa de acordo com a gravidade
    for(i= 0; i < NPIXELS; i++){
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
    for (i = 0; i < 2; i++){
      updateTime(i);
    }

    for(i = 0; i < NPIXELS; i++){
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
      loop1++;                  // incrementando a volta do player 1
      
      updateTurn(0, loop1);       // atualizando as voltas no LCD

      tone(PIN_AUDIO, 600);   // toca o buzzer avisando que uma volta foi concluída
      TBEEP = 2;
    };

    if (dist2 > NPIXELS * loop2) {
      loop2++;                  // incrementando a volta do player 2

      updateTurn(1, loop2);       // atualizando as voltas no LCD

      tone(PIN_AUDIO, 700);   // toca o buzzer avisando que uma volta foi concluída
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
      if (draworder==0) draworder=1;
      else draworder=0; 
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
      if (winner == 0)
        timeWinner = (minutes * 60000) + (sec * 1000) + (milSec * 100);

      winner = 1;

      for(i = 0; i < 3; i++){
        track.setPixelColor(i, COLOR1);
      }; 
    }
    else if(finished2){
      if (winner == 0)
        timeWinner = (minutes * 60000) + (sec * 1000) + (milSec * 100);

      winner = 2;
      
      for(i = 0; i < 3; i++){
        track.setPixelColor(i, COLOR2);
      }; 
    }
        
    track.show(); 
    delay(tdelay);

    if (TBEEP > 0) {
      TBEEP -= 1;
      
      // conflito de lib !!!! interrupção por neopixel
      if (TBEEP == 0)
        noTone(PIN_AUDIO);
    };
  }
  else{
    if(clear == 0){
      lcd.clear();
      clear = 1;
    }
    
    lcd.setCursor(5, 0);
    lcd.print("Press");
    lcd.setCursor(4, 1);
    lcd.print("To start");        
  }

}
