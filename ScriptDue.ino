#include <DueTimer.h>
#include <Wire.h>
#include <Adafruit_INA219.h>


Adafruit_INA219 ina219;

//Variaveis para interrupçao de timer
bool flag = false;
volatile bool horaDeAmostrar = false;
unsigned long testeTempo=0;

//Variaveis para interrupção pino externo
const byte interruptPin = 31;
volatile byte external_state = LOW;
volatile int cont_state = 0;
byte external_state_old=LOW;

int16_t medida_unitaria, medida_unitaria_old=1;

void amostrador(){  
  horaDeAmostrar=true;
}

void external_interrupt() {
  external_state = !external_state;
  cont_state++;
}

void setup(){
  SerialUSB.begin(2000000); //Inicia USB nativa
  ina219.begin(); //Inicia INA219
  ina219.setCalibration_Igor_32V_2A(); //Configura INA 219 para maior taxa de amostragem com leitura máxima de 2A
  Wire.setClock(2000000); //Seta velocidade da I2C para 2MHz
 
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), external_interrupt, CHANGE); //Cria interrupção externa para troca de estado

	Timer3.attachInterrupt(amostrador); //Cria uma interrupção pro timer
	// Timer3.start(1000000); // chama interrupção a cada 100us

  
}

void loop(){

  if (external_state != external_state_old)
  {
    if(cont_state==1)
    {
      //Inicio da aferição de dados...
      //Inicia timer da amostragem e envia "0000" pra USB
    // Timer3.start(1000000);// 1 segundo para testes
      Timer3.start(100);// 100 us 
    }
    else if(cont_state==9)
    {
      //Fim da conexão (envio do dado mqtt)
      //Para o timer de amostragem... aguarda nova conexão
      Timer3.stop();
      cont_state=0;
    }
    //Se houve troca de estado do pino 31, envia "0000" indicando fim de alguma etapa de conexão
   // SerialUSB.println(cont_state);
    SerialUSB.println("0000");
    external_state_old=external_state;
  }

  if(horaDeAmostrar)
  { 
 //     testeTempo=micros();
      medida_unitaria=ina219.getCurrent_raw();
 //     testeTempo=micros()-testeTempo;
 //     SerialUSB.println(testeTempo); 

      if(medida_unitaria<1) 
      {     
        SerialUSB.println(medida_unitaria_old);
      //  medida_unitaria_old = medida_unitaria_old;
      }
      else
      {
        SerialUSB.println(medida_unitaria);
        medida_unitaria_old=medida_unitaria;
      }
      
  //    SerialUSB.print(",");
  //    testeTempo=micros()-testeTempo;
  //    SerialUSB.println(testeTempo);
 //     n++;
    
    horaDeAmostrar=false;
  }

}
