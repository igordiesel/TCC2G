#include <TimerOne.h>
#include <SoftwareSerial.h>
#include "SIM800L.h"

/SoftwareSerial mySerial(10, 11); // RX, TX

/****MQTT CONNECTION DATA****/
#define MQTT_BROKER_URL "\"199.8.11.12\""
#define MQTT_BROKER_PORT 1883 
#define MQTT_DEVICE_NAME "\"examplesim800L\""
#define MQTT_CONN_USER "\"xxx\""
#define MQTT_CONN_PASS "\"xxx\""
#define MQTT_PUB_TOPIC "\"xxxx\""
/***************************/

/****AT COMMANDS STRINGS****/
#define STRING_VIRGULA ","

/***************************/

typedef enum {
  StateMachine_SIM800L_Off = 0,
  StateMachine_SIM800L_PowerOn,
  StateMachine_SIM800L_Initialized, 
  StateMachine_SIM800L_Check_SIM, 
  StateMachine_SIM800L_Check_Network_CREG,
  StateMachine_SIM800L_Check_Network_CGREG,
  StateMachine_SIM800L_Open_TCP,

  StateMachine_SIM800L_Wait_For_Close_MQTT_Con,
  StateMachine_SIM800L_CIPSHUT,
  StateMachine_SIM800L_Disable_All, //Disables all functionallity (CFUN=0). 
  
  StateMachine_SIM800L_MQTT_Config,
  StateMachine_SIM800L_MQTT_Check_Open,
  StateMachine_SIM800L_MQTT_Open,
  StateMachine_SIM800L_MQTT_Check_Connection,
  StateMachine_SIM800L_MQTT_Connect,
  StateMachine_SIM800L_MQTT_Publish,
  StateMachine_SIM800L_MQTT_Check_Publish, 

  StateMachine_Error=99,
  StateMachine_Test,
  StateMachine_Idle,
}StateMachine;
 
char RETBUFFER[256];
int buflenght=0, onFlag=0, error_Count=0, failsCount=0, cont=0, resend_MQTT=1;
unsigned int timeOutCounter=0, timeOutPublish=0;
bool resendPacket=false,publish_Topic=false;
StateMachine estado_Atual=StateMachine_SIM800L_Off, estado_Anterior=StateMachine_SIM800L_Off;

int pinState = LOW;   
int pinCount=0;
int cont_Data_Sent=1;
int error_Flag=0;
unsigned int msg_ID_atual=0;




void Start_Timer(long int Time_us)
{
  Timer1.initialize(Time_us);  
  Timer1.attachInterrupt(TimerOneInterrupt); 
}

void TimerOneInterrupt()
{
  timeOutCounter++; //Aumenta a cada 5ms
  timeOutPublish++; //User for MQTT publish only
}



void setup() {
  //Pino digital de saida
  pinMode(8, OUTPUT);

  //UARTS
  Serial.begin(9600); //Serial da USB (para Debug e Logging)
  mySerial.begin(9600); //mySerial de comunicação com SIM800L (Comandos AT)

  //Timer
  Start_Timer(5000); //Inicia timer para TIMEOUT
  
  estado_Atual=StateMachine_Idle; // StateMachine_SIM800L_PowerOn;
  
  while(!Serial);
}

void loop() {
  // read from port 1, send to port 0:
  int i=0, campo1, campo2, campo3, campo4, simCard;
  unsigned int puiAddres, puiLenght, addIniPacket;
  int pbStatus;
  int add_ini, k=0,hh=0; //variaveis de teste que podem ser apagadas posteriormente.....

  puiAddres=0;
  puiLenght=0;
  pbStatus=0;
  addIniPacket=0;
  SIM800L.flushBuffer(RETBUFFER,256);
  
  switch (estado_Atual)
  {
/******************************************************************/
    case StateMachine_SIM800L_PowerOn:
      if((estado_Anterior!=estado_Atual)||resendPacket) //Se é a primeira vez que passa aqui ou precisa dar um resend...
      {
        mySerial.println("AT+CFUN=1"); //Inicia CFUN
        
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
        error_Flag=0;
        pinState=!pinState;
        digitalWrite(8, pinState);
        pinCount++;
        Serial.print("**********************  ");
        Serial.println(pinCount);
        delay(4);
        // tries=0;
      }

      if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        SIM800L.fillBuffer(RETBUFFER,&buflenght);     
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "OK");      

       // Serial.print("........... fim enche buffer, pbstatus = ");
       // Serial.println(pbStatus);
        if(pbStatus==1)
        {
          Serial.print(F("ok detectado no cfun=1)"));
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_SIM800L_Initialized; //ToDo: fechar as conexões antes de abrir de novo?   (CFUN)      
        }                
      }

      
     if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 2000)) //Mesmo estado a mais de 10s
     {
          Serial.println("Timeout");
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_Error; 
      }
    
      break;
/******************************************************************/
    case StateMachine_SIM800L_Initialized:
    //Espera SMS ready e CALL ready que indica modulo inicializado

      if((estado_Anterior!=estado_Atual)||resendPacket) //Se é a primeira vez que passa aqui ou precisa dar um resend...
      {       
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
        // tries=0;
      }
    
      if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        SIM800L.fillBuffer(RETBUFFER,&buflenght);             
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "SMS");          
        if(pbStatus==1)
        {
          //Recebeu SMS Ready  
          //Module is powered on and SMS initialization procedure is over. 
        }
        else
        {
          pbStatus=SIM800L.findInBuffer(RETBUFFER, "Call"); 
          if(pbStatus==1)
          {
            // Recebeu CALL Ready, pode conectar CREG
            //Module is powered on and phonebook initialization procedure is over. 
            estado_Anterior=estado_Atual;
            estado_Atual=StateMachine_SIM800L_Check_Network_CREG; 
           // Serial.print("CALL READY DETECTADO");
            pinState=!pinState;
            digitalWrite(8, pinState);
            pinCount++;
            Serial.print("**********************  ");
            Serial.println(pinCount);
          }   
        }                
      }

   if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 1000)) //Mesmo estado a mais de 10s
   {
        Serial.println(F("Timeout INITIALIZED"));
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_SIM800L_Check_Network_CREG; //StateMachine_Idle; 

        pinState=!pinState;
        digitalWrite(8, pinState);
        pinCount++;
        Serial.print("**********************  ");
        Serial.println(pinCount);
    }
    
    break;
/******************************************************************/
    case StateMachine_SIM800L_Check_Network_CREG:
    
      // When we are sure that the sim is connected, check network status and wait to connect..
      // ToDo: Add TRIES...
      
      if((estado_Anterior!=estado_Atual)||resendPacket) //Se é a primeira vez que passa aqui ou precisa dar um resend...
      {
        mySerial.println("AT+CREG?");
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        error_Count=0;
        resendPacket=false;
      }


      if(!mySerial.available())
      {
        
      }
      else
      { 
        SIM800L.fillBuffer(RETBUFFER,&buflenght);
        SIM800L.get_Package_Data(RETBUFFER,"CREG", &pbStatus, &campo1, &campo2, NULL, NULL); //fills campo1 and campo2 just if "CREG" is found.
  
        if (pbStatus==1) //resposta tem "CREG"
        {
          
//          Serial.print(" campo 2 no CREG: ");
//          Serial.print(campo1);
//          Serial.print("  ");
//          Serial.println(campo2);

          switch (campo2)
          {
            case 0:
              Serial.println(F("Not Registered. Not searching.")); //A Antena esta conectada?
              break;
            case 1:
              Serial.println(F(" Registered in CREG Succesfully "));
              estado_Anterior=estado_Atual;
              estado_Atual=StateMachine_SIM800L_Check_Network_CGREG; //StateMachine_SIM800L_Check_Network_CGREG; //StateMachine_Error;
              pinState=!pinState;
              digitalWrite(8, pinState);    
              pinCount++; 
              Serial.print("**********************  ");
              Serial.println(pinCount);         
              break;
            case 2:
              Serial.println(F(" SEARCHING FOR NETWORK "));
              break;
            case 3:
              Serial.println(F(" REGISTRATION DENIED "));
              //Todo: isso aconteceu!! o q fazer?
              break;
            case 4:
              Serial.println(F(" unknown "));
              break;
            case 5:
              Serial.println(F(" Registered in Roaming "));
              break;
            default:
              break;
          }

        }
        else //Não encontrou "CREG", pde ser um URC ou ERROR...   
        {
          Serial.println(F("N encontrou CREG"));
        }
      }


      // O timer abaixo vai estourar quando:
      // - Nao houver resposta a mais de 10 segundos
      // - A resposta indicar que ainda esta conectando, aguardando 10 segundos para reenviar o comando

      if (error_Count>=120) //Se tentar mais de 1 minutos reseta....
      {        
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Error; 
      }
      else if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 100)) //Mesmo estado a mais de 0.5s, resend packet....
      {
        // Todo: Adicionar um "Tries" aqui? se tentar mandar o comando
        // varias vezes e nao tiver resposta, o q fazer?
        resendPacket=true;
        error_Count++;
        Serial.println(F("REENVIAR CREG"));
      }

      break;
 /****************************************************************************/           
    case StateMachine_SIM800L_Check_Network_CGREG: //Similar to CREG     
      
      if((estado_Anterior!=estado_Atual)||resendPacket) //Se é a primeira vez que passa aqui ou precisa dar um resend...
      {
        mySerial.println("AT+CGREG?");
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
        error_Count=0;
      }


      if(!mySerial.available())
      {
        
      }
      else
      { 
        SIM800L.fillBuffer(RETBUFFER,&buflenght);
        SIM800L.get_Package_Data(RETBUFFER,"CGREG", &pbStatus, &campo1, &campo2, NULL, NULL); //fills campo1 and campo2 just if "CGREG" is found.
          
        if(pbStatus==1) //Found "CGREG"
        {
          Serial.print(" CGREG:  ");
          
          switch (campo2)
          {
  
            case 0:
              Serial.println(F("Not Registered. Not searching."));
              break;
            case 1:
              Serial.println(F(" Registered in CGREG Succesfully "));
              estado_Anterior=estado_Atual;
              estado_Atual=StateMachine_SIM800L_Open_TCP; //StateMachine_SIM800L_Check_Network_Info; //StateMachine_Error;
              pinState=!pinState;
              digitalWrite(8, pinState); 
              pinCount++; 
              Serial.print("**********************  ");
              Serial.println(pinCount);     
              break;
            case 2:
              Serial.println(F(" SEARCHING FOR NETWORK "));
              break;
            case 3:
              Serial.println(F(" REGISTRATION DENIED "));
              break;
            case 4:
              Serial.println(F(" unknown "));
              break;
            case 5:
              Serial.println(F(" Registered in Roaming "));
              break;
            default:
              break;          
          }
        }
        else
        {
          //CGREG not found. URC or Error? //Todo: Manage URC and Error. 
        }
        
      }

      // O timer abaixo vai estourar quando:
      // - Nao houver resposta a mais de 10 segundos
      // - A resposta indicar que ainda esta conectando, aguardando 1 segundos para reenviar o comando

      if (error_Count>=120)
      { //Mais de 1 min tentando, reseta
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Error; 
      }
      else if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 100)) //Mesmo estado a mais de 0.5s, resend packet....
      {
        // Todo: Adicionar um "Tries" aqui? se tentar mandar o comando
        // varias vezes e nao tiver resposta, o q fazer?
        resendPacket=true;
        error_Count++;
        Serial.println(F("REENVIAR comando"));
      }


      break;
/****************************************************************************/     
    case StateMachine_SIM800L_Open_TCP:
         
      if((estado_Anterior!=estado_Atual)||resendPacket) //Se é a primeira vez que passa aqui ou precisa dar um resend...
      {
        mySerial.println("AT+CIPSTART=\"TCP\",\"m24.cloudmqtt.com\",\"13933\""); //open TCP conect with mqtt
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
        error_Count=0;
      }  
      
      if(mySerial.available())
      {
        
        SIM800L.fillBuffer(RETBUFFER,&buflenght);           
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "CONNECT");          
        if(pbStatus==1)
        {
          //Recebeu CONNECT OK do comando, vai pro proximo estado
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_SIM800L_MQTT_Connect;//StateMachine_SIM800L_MQTT_Connect;   
          pinState=!pinState;
          digitalWrite(8, pinState);
          pinCount++;
          Serial.print("**********************  ");
          Serial.println(pinCount);
          Serial.println(F("CONNECT OK do cipstart"));             
        }
        else
        {
          pbStatus=SIM800L.findInBuffer(RETBUFFER, "OK"); 
          if(pbStatus==1)
          {
            //Recebeu OK do comando, 
            Serial.println(F("OK do cipstart"));          
          }
          else
          {
            pbStatus=SIM800L.findInBuffer(RETBUFFER, "CONNECT FAIL");
            if(pbStatus==1)
            {
              //Falha da conexao, tenta de novo...
              resendPacket=true;
              Serial.println(F("FAIL CONNECT do cipstart"));          
            }               
          }
        }
      }

     if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 4000)) //Mesmo estado a mais de 20s (2000 = 10S)
     {
          Serial.println("Timeout DO CIPSTART");
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_Error; 
      }
    
      break;
/****************************************************************************/      
    case StateMachine_SIM800L_MQTT_Connect: //Connect to MQQT broker after the TCP connection is OPEN

      if((estado_Anterior!=estado_Atual)||resendPacket)
      {
        Serial.println(F("Inicia conexao com o mqtt..."));
        //(connectMQTT(nomeCLiente, flagUser, flagPass, User, Pass, 1, 0 0 0...)
        SIM800L.connectMQTT("qwertyuiop", 1, 1, "rfklobub", "TjsBcIUSxLM3", 1, 0, 0, 0, "", "");
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
      }

      if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        int t=0;
        SIM800L.fillBuffer(RETBUFFER,&buflenght);
        // A resposta do ACK quando conecta o mqtt tem 4 bytes:
        // 32 2 0 0 
        // Quando da erro de usuario/pass fica
        // 32 2 0 5
        // Pra nao complicar, vou testar o primeiro e o ultimo na mão grande...

        if(RETBUFFER[0]==32)
        {
          if(RETBUFFER[3]==0)
          {
            Serial.println(F("Conectado no mqtt com sucesso"));
            estado_Anterior=estado_Atual;
            estado_Atual=StateMachine_SIM800L_MQTT_Publish;//StateMachine_SIM800L_MQTT_Connect;   
            pinState=!pinState;
            pinCount++;
            digitalWrite(8, pinState);
            Serial.print("**********************  ");
            Serial.println(pinCount);
          }
          else
          {
            Serial.println(F("Erro de conexão Usuário MQTT"));
          }
        }
        else
        {
          Serial.println(F("Nao reconheci nem o 32"));
        }
                
//        Serial.println("print Ack para ver o que vem no ACK (32 2 0 0)");
//        for (t=0;t<(buflenght);t++)
//        {
//         Serial.print(RETBUFFER[t],DEC);
//         Serial.print(" ");
//        }
//        Serial.println();    
      }        

      if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 4000)) //Mesmo estado a mais de 10s, resend packet....
      {
        //ToDo: Tries?
  //      resendPacket=true;
        Serial.println("REENVIAR comando?");
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Error;  
      }
      
      break;
 /****************************************************************************/           
    case StateMachine_SIM800L_MQTT_Publish: //Publish any MQTT topic

      if((estado_Anterior!=estado_Atual)||resendPacket)
      {        
        char char_toSend[5];
        String str;
        str=String(cont_Data_Sent);
        str.toCharArray(char_toSend,5);
        
        
        //publishMQTT(char DUP, char Qos, char RETAIN, unsigned int MessageID, char *Topic, char *Message);
        if (resendPacket==true)
        {
          SIM800L.publishMQTT(0, 1, 0, msg_ID_atual, "TCCGPRS", char_toSend);
          Serial.print(F("Envia dado para o MQTT RESEND, msgID = "));
          Serial.println(msg_ID_atual);
        }
        else
        {
          msg_ID_atual=SIM800L._generateMessageID();
          SIM800L.publishMQTT(0, 1, 0, msg_ID_atual, "TCCGPRS", char_toSend);
          Serial.print(F("Envia dado para o MQTT, msgID = "));
          Serial.println(msg_ID_atual);
        }
        resendPacket=false;
        timeOutCounter=0; //Reseta contador de timeout 
        estado_Anterior=estado_Atual;
      }

      if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        int t=0;
        SIM800L.fillBuffer(RETBUFFER,&buflenght);
        // A resposta do ACK quando envia o dado pro mqtt tem 4 bytes:
        // 64 2 0 X 
        // Onde X é o ID da mensagem que recebeu ACK.. (calculado pelo _generateMessageID());
        // Pra nao complicar, vou testar o primeiro byte e tudo bem

        if(RETBUFFER[0]==64)
        {
          Serial.println(F("Enviou o dado corretamente"));
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_SIM800L_Wait_For_Close_MQTT_Con; //StateMachine_SIM800L_Disable_All;//StateMachine_SIM800L_MQTT_Connect;   
          pinState=!pinState;
          digitalWrite(8, pinState);
          pinCount++;
          cont_Data_Sent++;
          resend_MQTT=1;
            Serial.print("**********************  ");
          Serial.println(pinCount);
         
    
          // Debug para ver o pacote do ACK
          Serial.print(F("print o pacote de  ACK só pra ver o que tem dentro :  "));
          for (t=0;t<(buflenght);t++)
          {
           Serial.print(RETBUFFER[t],DEC);
           Serial.print(" ");
          }
          Serial.println();   
          
        }
        else
        {
          Serial.println(F("Nao reconheci nem o 64"));
          Serial.print(F("print o pacote de  ACK só pra ver o que tem dentro :  "));
          for (t=0;t<(buflenght);t++)
          {
           Serial.print(RETBUFFER[t],DEC);
           Serial.print(" ");
          }
          Serial.println();  
          // Provavelmente n enviou o dado e recebeu um CLOSED em vez do ACK...
          // Assim pulamos o closed e vamos direto pro CIPSHUT
          pbStatus=SIM800L.findInBuffer(RETBUFFER, "CLOSED");          
          if(pbStatus==1)
          {
            estado_Anterior=estado_Atual;
            estado_Atual=StateMachine_Error; //StateMachine_SIM800L_Disable_All;//StateMachine_SIM800L_MQTT_Connect;   
//            pinState=!pinState;
//            digitalWrite(8, pinState);
//            delay(5);
//            pinState=!pinState;
//            digitalWrite(8, pinState);
//            pinCount++;
          }
          
        }
      }        

      if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 300)) //Não recebeu ACK em 1 segundo, manda de novo... (cada 100 é 500ms)
      {
        //ToDo: Tries?
        resendPacket=true;
        Serial.println(F("REENVIAR comando PUBLISH?"));
  //      estado_Anterior=estado_Atual;
 //       estado_Atual=StateMachine_Idle; //StateMachine_Error;  
 //       cont_Data_Sent++;
      }
               
      break;
 /****************************************************************************/   
   case StateMachine_SIM800L_Wait_For_Close_MQTT_Con:

      if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        SIM800L.fillBuffer(RETBUFFER,&buflenght);             
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "CLOSED");          
        if(pbStatus==1)
        {
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_SIM800L_CIPSHUT; //StateMachine_SIM800L_Disable_All;//StateMachine_SIM800L_MQTT_Connect;   
          pinState=!pinState;
          digitalWrite(8, pinState);
          pinCount++;
          Serial.print("**********************  ");
          Serial.println(pinCount);
        }
        else
        {
          Serial.println(F("Deveria ter reconhecido um CLOSED pro TCPIP aqui...."));
        }                
      }

      if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 2000)) //Mesmo estado a mais de 10s, resend packet....
      {
        //ToDo: Tries?
  //      resendPacket=true;
        Serial.println(F("TIMEOUT AGUARDANDO CLOSED TCP"));
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Idle; //StateMachine_Error;  
        cont_Data_Sent++;
      }

      break;
 /****************************************************************************/  
    case StateMachine_SIM800L_CIPSHUT:
      //Fecha conexao TCP e reinicializa IP (simula desligar o módulo)

     if((estado_Anterior!=estado_Atual)||resendPacket)
      {
        Serial.println("-- CIPSHUT");
        mySerial.println("AT+CIPSHUT");
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
      }


       if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        SIM800L.fillBuffer(RETBUFFER,&buflenght);             
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "OK");          
        if(pbStatus==1)
        {
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_SIM800L_Disable_All;
        //  pinState=!pinState;
      //    digitalWrite(8, pinState);
        //ToDo: COnsiderar CIPSHUT no calculo?
        }
        else
        {
          Serial.println(F("Deveria ter recebido OK do CFUN aqui..."));
        }                
      }

      if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 12000)) //Wait 1 minute to continue 
      {
        //ToDo: Tries?
  //    resendPacket=true;
        Serial.println("Timeout CFUN");
        
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Idle; //StateMachine_SIM800L_MQTT_Publish; //StateMachine_Error;
       // delay(1000);  
      }

      break;

/****************************************************************************/  
        
    case StateMachine_SIM800L_Disable_All:
      
      if((estado_Anterior!=estado_Atual)||resendPacket)
      {
        Serial.println("--CFUN=0");
        mySerial.println("AT+CFUN=0");
        estado_Anterior=estado_Atual;
        timeOutCounter=0; //Reseta contador de timeout
        resendPacket=false;
      }


       if(!mySerial.available())
      {
       //Resposta ainda nao chegou...
      }
      else
      {
        SIM800L.fillBuffer(RETBUFFER,&buflenght);             
        pbStatus=SIM800L.findInBuffer(RETBUFFER, "OK");          
        if(pbStatus==1)
        {
          estado_Anterior=estado_Atual;
          estado_Atual=StateMachine_Idle;
          if(error_Flag==0)
          {
            pinState=!pinState;
            digitalWrite(8, pinState);
            pinCount++;
            Serial.print("**********************  ");
            Serial.println(pinCount);
          }
          else
          {
            Serial.print("***n deu error flag0*******  ");
            Serial.println(pinCount);
          }
        }
        else
        {
          Serial.println(F("Deveria ter recebido OK do CFUN aqui..."));
        }                
      }

      if ((estado_Atual==estado_Anterior) && (timeOutCounter >= 12000)) //Wait 1 minute to continue 
      {
        //ToDo: Tries?
  //    resendPacket=true;
        Serial.println("Timeout CFUN");
        estado_Anterior=estado_Atual;
        estado_Atual=StateMachine_Idle; //StateMachine_SIM800L_MQTT_Publish; //StateMachine_Error;
       // delay(1000);  
      }
     

      break;
 /****************************************************************************/           
    case StateMachine_Test: //Tests.....

    
    
      estado_Anterior=estado_Atual;
      estado_Atual=StateMachine_Idle;
      break;
 /****************************************************************************/          
    case StateMachine_Error:
    // Caso deu algum error reseta tudo
    
    error_Flag=1;
    error_Count=0;

    Serial.print("*****NO ERRO*****************  ");
    Serial.println(pinCount);
    if(pinCount<9)
    {
      for(hh=0;(hh+pinCount)<9;hh++)
      {
         pinState=!pinState;
         digitalWrite(8, pinState);
         delay(200);
      }
    }
    pinCount=0;
    delay(10000); //Delay de 10 segundos pra dar tempo de vir todas as msg

    estado_Anterior=estado_Atual;   
    estado_Atual=StateMachine_SIM800L_CIPSHUT; //To ensure that all connections are ok, test everything before publish  
    
    break;

/****************************************************************************/      
    case StateMachine_Idle:
      // Depois de executar as funções padroes de configuração,
      // entra no estado IDLE, que é o padrão de testes, printando
      // da Serial pra mySerial e vice-versa.

           

    
      // read from port 0, send to port 1:
      // Useful for inserting AT comands Manually in the SM (Serial Monitor)
     /* Os comandos AT devem ser enviados sempre com o <CR><LF> no fim. 
      * Para inserir o CR LF podemos: 
      * -> printar "\r\n"
      * -> usar serial.println 
      * -> para comandos enviados pelo monitor serial, selecionar 
      * final de linha como NL e CR. Neste caso não precisa usar nem println
      * nem \r\n, pois o SM insere \r\n automaticamente.
      * 
      */   
      error_Flag=0;
      pinCount=0;


      if (mySerial.available()) //May be useful for URC messages
      {
        // Prints into SM (Serial Monitor) anything that comes in the AT_Uart
        SIM800L.fillBuffer(RETBUFFER,&buflenght); //Fills buffer and Print response in the Serial Monitor.
      }
      
      if (Serial.available())
      {
        while (Serial.available())
        {
          char inByte = Serial.read();    
                
          if(inByte=='*')
          {
   
          }
          else if(inByte=='7')
          {
            mySerial.write("AT+CSTT=\"AIRTELGPRS.COM\"\r\n"); //open TCP conect with mqtt
          }
          else if(inByte=='2')
          {
            mySerial.write("AT+CIPSTART=\"TCP\",\"m24.cloudmqtt.com\",\"13933\""); //open TCP conect with mqtt
          }
          else if(inByte=='3')
          {            
            //SIM800L.checkSIM();
            estado_Anterior=estado_Atual;
            estado_Atual=StateMachine_SIM800L_PowerOn;
          }
          else
          {    
             mySerial.print(inByte);   
           //  Serial.print(inByte);  
          }
        }
      }      
      else if((timeOutPublish>=36000))//&&(resend_MQTT==1)) //Publica MQTT a cada 2 minutos //resend_MQTT para dar resend só se o outro deu certo
      {
      //  if (resend_MQTT==1) //Last package was sent succesfully... If not, what Todo? Store values and send later? 
     //   {
          cont++;
          timeOutPublish=0;
          estado_Anterior=estado_Atual;   
          estado_Atual=StateMachine_SIM800L_PowerOn; //To ensure that all connections are ok, test everything before publish  
          publish_Topic=1; //Used inside other states
          resend_MQTT=0;    
          Serial.println(F(" TIMER DE PUBLICACAO NO LOOP"));
    //    }
      }
      break;
 /****************************************************************************/           
    default:
      //
      break;
  }
  
}
