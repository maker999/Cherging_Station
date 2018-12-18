#include <sha256.h>
#include <ArduinoJWT.h>

#include <WiFiServerST.h>
#include <WiFiClientST.h>
#include <ArduinoHttpClient.h>
//#include <HttpClient.h>
//#include <aREST.h>
//aREST rest = aREST();

#include <ArduinoJson.h>

#include "wifi.h"
#include "crypino.h"
#include "modbus.h"
#include "b64.h"

#include <Wire.h>
#include <M24SR.h>

/*  NFC M24SR Module Conifguration */
#define I2C2_SCL        PB10
#define I2C2_SDA        PB11
#define M24SR_ADDR      0xAC
#define GPO_PIN         PE4
#define RF_DISABLE_PIN  PE2

TwoWire dev_i2c(I2C2_SDA, I2C2_SCL);
M24SR nfcTag(M24SR_ADDR, &dev_i2c, NULL, GPO_PIN, RF_DISABLE_PIN);
/*----------------------------*/

ATCAIfaceCfg *gCfg = &cfg_ateccx08a_i2c_default;
ATCA_STATUS crypino_status = ATCA_GEN_FAIL;

//char serverAddress[] = "middleware.riddleandcode.com";
char serverAddress[] = "ipdb-eu1.riddleandcode.com";
int port = 9984;


WiFiClient wificlient;
WiFiServer wifiserver(80);
//HttpClient client = HttpClient(wificlient, serverAddress, port);

ArduinoJWT jwt1 = ArduinoJWT("");

//DynamicJsonBuffer jsonBuffer;
StaticJsonBuffer<2000> jsonBuffer;
JsonObject& jsonTX = jsonBuffer.createObject();


unsigned int reg[10];
char msg[42];
uint8_t pk[64]={0};
uint8_t pubkey[65]={0x04};
String keystr = "";

String byte2string(uint8_t* bytarr, uint8_t len);
String post_http(const char* path,const char* pubkey,String postData);
void hex2byte(const char* respc, uint8_t* hx, uint8_t len);

double kwh0,kwh1;
String postData,response;
String txpk;

void setup() {


  // initialize serial communication:
  Serial.begin(115200);
  while (!Serial) {
     // wait for serial port to connect. Needed for native USB port only
  }

//  rest.set_id("008");
//  rest.set_name("dapper_drake");
  //rest.function("balance_check",blc_check );

  Serial.println("\n\r-----------------------------------------\n\rStart");

  // Intialize NFC module
  dev_i2c.begin();
  if(nfcTag.begin(NULL) != 0) Serial1.println("NFC Init failed!");
  else Serial1.println("NFC init successful.");

  crypino_status = atcab_init(gCfg);

  modbus_init();
  
  while(kwh0 == 0.0) kwh0 = modbus_read_kwh();

  Serial.println(kwh0);

//  uint8_t i = 0;
  do{
     atcab_get_pubkey(0, pk);
  }while(pk[13]==0);      //while(crypino_status != ATCA_SUCCESS);

  Serial.println("secure element:");

  String pubkeystr = byte2string(pk,64);
  Serial.println(pubkeystr);

  uECC_compute_public_key(pk, pubkey+1 , uECC_secp256r1() );

  keystr = byte2string(pubkey,65);

  String ip = wifi_init();


  Serial.print("-----my pubkey computed:");
  Serial.println(keystr);
  Serial.print("IP Address:");
  Serial.println(ip);
  String nfc_text = keystr + "," + ip;
  if(nfcTag.writeTxt(nfc_text.c_str()) == false) Serial1.println("NFC Write Failed!");
  else Serial.println("Pubkey is written on the NFC");

  Serial.println("-----");

/*
  jsonTX["public_key"]= keystr;

  jsonTX.printTo(postData);

  response = post_http("/v1/key-exchange", 0 , postData.c_str() );

  JsonObject& jsonRX = jsonBuffer.parseObject( response );

  Serial.println("----server's pubkey is received:");
//  jsonRX.prettyPrintTo(Serial);

  Serial.println(jsonRX["data"]["public_key"].as<String>());

  const char* respc = jsonRX["data"]["public_key"];



  uint8_t hx[65]={0x04};
  uint8_t secret[32]={0};

//  char resp[174]={0};
//  response.toCharArray(resp,174);
  hex2byte(respc,hx,65);
  uECC_shared_secret( hx+1, pk , secret, uECC_secp256r1());

  jwt1.setPSK(byte2string(secret,32));

  Serial.print("\n\rsecret: ");
  Serial.println(byte2string(secret,32));
*/
  wifiserver.begin();
}
/*
void ChargeRequestFathomhash()
{
  Serial.println("charge request.");
  //memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+22),128);
  memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+14),128);
  //user_pk[128]=0;
  //Serial.println((unsigned int)charge,HEX);
  Serial.println(user_pk);
  String user_pk_str = String(user_pk);
  //postData ="{\"public_key\":\"" + user_pk_str + "\"}";
  postData ="{\"public_key\":\"CJL6QoHLS9vmWfk5zRi7qQc6WrEmp2Jh8UpgWMaxHctK\"}";
  //postData = "{\"token\":\"" + jwt1.encodeJWT(postData) + "\"}";
  response = post_http("/api/v1/outputs" , NULsL , postData);
  jsonBuffer.clear();
  JsonObject& jsonRX8 = jsonBuffer.parseObject(response);
  //tkn = jsonRX8["token"].as<String>();
  jwt1.decodeJWT(tkn,response);
  Serial.println(response);
  JsonObject& jsonRX9 = jsonBuffer.parseObject(response);

  if( jsonRX9["metadata"]["balance"].as<String>().toFloat() != 0.0 ){
    wcl.println("HTTP/1.1 201 OK");
  }else wcl.println("HTTP/1.1 299 OK");

  wcl.println("Content-type:application/json");
  wcl.println("Connection: close");
  wcl.println();
  wcl.println(response);
  break;
}
*/

void loop() {

//  char text_read[100]={'\0'};
//  nfcTag.readTxt(text_read);
//  Serial.print("Message content: ");
//  Serial.println(text_read);
//
//
//  delay(1000);
//  return;

  //---------------------------------------------------------------------------------------
  String tkn;

  WiFiClient wcl = wifiserver.available();       // listen for incoming clients

  char user_pk[129]={0};
  if(wcl){
      Serial.println("New server available.");
      uint8_t buf[10000]={0};
      int c = 0, cur = 0;
      while(wcl.connected()){
          if(wcl.available()){
            Serial.println("New Client.");

            do{
                c = wcl.read(buf + cur, 500);
                cur += c;
                if(c<500) break;
            }while(c != 0);

            buf[cur] = 0;
            Serial.print("wifi buffer:");
            Serial.println((char*)buf);
            //char* charge = strstr((char*)buf,"GET /charge/?pubkey=");
            char* charge = strstr((char*)buf,"GET /charge/");
            char* invoice = strstr((char*)buf,"GET /invoice/");

            if (charge != 0) {
              postData = "";
              Serial.println("output request.");
              //memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+22),128);
              memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+20),44);
              //user_pk[128]=0;
              //Serial.println((unsigned int)charge,HEX);
              Serial.println(user_pk);
              String user_pk_str = String(user_pk);
              //response = get_http(String("/api/v1/outputs?public_key=") + user_pk_str);
              jsonBuffer.clear();
              //String obj("{resp=");
              //obj = obj + response + String("}");
              JsonArray& jsonRX8 = jsonBuffer.parseArray(response);
              jsonRX8.prettyPrintTo(Serial);
              Serial.println();
              String tx_id = jsonRX8[jsonRX8.size()-1]["transaction_id"].as<String>();
              Serial.print("transaction request. ");
              Serial.println(tx_id);
              //response = get_http(String("/api/v1/transactions/")+ tx_id);
              response = get_http("/api/v1/transactions/cfd0a86ea21ff96e48aa5ebd3f5c802a6c98555a31e30580b70ce212899d02bc");
              //response = "{\"inputs\": [{\"owners_before\": [\"CJL6QoHLS9vmWfk5zRi7qQc6WrEmp2Jh8UpgWMaxHctK\"], \"fulfills\": {\"transaction_id\": \"e957a9d02cfdf2f090509e3385e7ebf7021a3c24e95d3fdb18ac3df4815c1225\", \"output_index\": 0}, \"fulfillment\": \"pGSAIKfhBM8pUHpggKBq4vyB0khGLVwXnrbcNZO_RL3VQBCcgUBfqicpyEjg8fQA6EnUDH5gbLvNfjAKPEbtCA_01Winyu_dmcrkSXbdD1lGW95SkE_22dcS9LRniUQIhbrG3DoI\"}], \"outputs\": [{\"public_keys\": [\"CJL6QoHLS9vmWfk5zRi7qQc6WrEmp2Jh8UpgWMaxHctK\"], \"condition\": {\"details\": {\"type\": \"ed25519-sha-256\", \"public_key\": \"CJL6QoHLS9vmWfk5zRi7qQc6WrEmp2Jh8UpgWMaxHctK\"}, \"uri\": \"ni:///sha-256;9ZZF8a5pvCL1Ln-tY0CdL2z-Z5d59NBy1-0Y8bJAa3Q?fpt=ed25519-sha-256&cost=131072\"}, \"amount\": \"999960\"}, {\"public_keys\": [\"FL2KzJwdYqZLLXBTTgAjU2B54hGs2AWhfEC7mnN73iSC\"], \"condition\": {\"details\": {\"type\": \"ed25519-sha-256\", \"public_key\": \"FL2KzJwdYqZLLXBTTgAjU2B54hGs2AWhfEC7mnN73iSC\"}, \"uri\": \"ni:///sha-256;NuV7DENpEHgFBdxWssuNNW_nlo9B0odIkgKse-hggIk?fpt=ed25519-sha-256&cost=131072\"}, \"amount\": \"20\"}], \"operation\": \"TRANSFER\", \"metadata\": {\"what2\": \"Transferring to bob...\"}, \"asset\": {\"id\": \"e957a9d02cfdf2f090509e3385e7ebf7021a3c24e95d3fdb18ac3df4815c1225\"}, \"version\": \"2.0\", \"id\": \"cfd0a86ea21ff96e48aa5ebd3f5c802a6c98555a31e30580b70ce212899d02bc\"}\r\n";
              response.trim();
              Serial.println(response);
              jsonBuffer.clear();
              JsonObject& jsonRX9 = jsonBuffer.parseObject(response);
              
              jsonRX9.prettyPrintTo(Serial);
              if( jsonRX9["outputs"][0]["amount"].as<String>().toInt() > 10 ){
                wcl.println("HTTP/1.1 201 OK");
              }else wcl.println("HTTP/1.1 299 OK");

              wcl.println("Content-type:application/json");
              wcl.println("Connection: close");
              wcl.println();
              wcl.println(response);
              break;

            }else if(invoice != 0){

              Serial.println("invoice request.");
              kwh1 = modbus_read_kwh();
              Serial.print("diff:");
              Serial.println(kwh1-kwh0,4);
              //float price = (kwh1 - kwh0) * 0.2;   //the real price calculator
              float price = (kwh0) * 0.2;
              wcl.println("HTTP/1.1 201 OK");
              wcl.println("Content-type:application/json");
              wcl.println("Connection: close");
              wcl.println();
              wcl.println("{\"price\":\"" + String(price) + "€\"}");
              Serial.println("{\"price\":\"" + String(price) + "€\"}");;
              kwh0 = kwh1;
              break;

            }
          }

      }
      wcl.stop();
      wifiserver.begin();
  }
  return;
}



//----------------------------------------------------------------------------------------------------------


String get_http(String path){
  HttpClient client = HttpClient(wificlient, serverAddress, port);

  client.get(path.c_str());
  
  String response = client.responseBody();

  
  //Serial.println(response);

  client.stop();
  return response;


}

String post_http(const char* path,const char* pubkey,String postData){
  HttpClient client = HttpClient(wificlient, serverAddress, port);
  client.beginRequest();
  client.post(path);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", postData.length() );
  if(pubkey != NULL ) client.sendHeader("X-Public-Key", pubkey );
  client.print(postData);
  client.endRequest();

  char buf[10000]={0};
  uint32_t response_size=0;
  uint32_t header_size=0;

  resp_read(buf, &response_size, &header_size);
  //Serial.print("buf=");
  //Serial.println(buf);

  String response;
  for(int i=0; i<response_size; i++){
    response.concat(buf[i+header_size]);
  }
  client.stop();
  return response;

}

int resp_read(char* response, uint32_t *response_size, uint32_t *header_size){
  uint8_t *buf = (uint8_t *)response;
  int c = 0, retries = 20, cur = 0, length;
//  Serial.println(" line417 ");
  do
    {
        c = wificlient.read(buf + cur, 100);
        cur += c;

//        Serial.print("cur = ");
//        Serial.println(cur);
        if(c<100) break;

    }while(c != 0);


    buf[cur] = 0;

//    wificlient.flush();
    length = cur;

    // Check status
    if (memcmp("HTTP/", buf, 5) != 0 || memcmp("200", buf + 9, 3) != 0)
    {
        Serial.println("Invalid HTTP response.");
        Serial.println(length);
        char status_code[4] = {0};

        memcpy(status_code, buf + 9, 3);
        for (uint8_t j = 0; j < 200; j++)
        {
            Serial.print((char)buf[j]);
        }
        return 1;
    }
    //Serial.println(" 478 ");
    // Find where body starts
    cur = 0;
    for (uint64_t i = 0; i < length - 4; i++)
    {
        if (memcmp(&buf[i], "\r\n\r\n", 4) == 0)
        {
            cur = i + 4;
            break;
        }
    }

    if (cur == 0)
    {
        return 1;
        Serial.println("\ncur=0");
    }

    *response_size = length - cur;
    *header_size = cur;


    return 0;

}


String byte2string(uint8_t* bytarr, uint8_t len){
  String str="";
  for (int i = 0; i < len; i++){
     static char tmp[2] = {};
     sprintf(tmp, "%02X", bytarr[i]);
     str += tmp;
  }
  return str;
}

void hex2byte(const char* respc, uint8_t* hx, uint8_t len){
  int tmp1 = 0;
  int tmp2 = 0;
  for (int i = 0; i < (len*2); i++){
    if(i%2 == 0){
      if(respc[i] > 64) tmp2=9+respc[i]-64 ;
      else if(respc[i] < 58) tmp2=respc[i]-48;
    }
    else{
      if(respc[i] > 64) tmp1=9+respc[i]-64 ;
      else if(respc[i] < 58) tmp1=respc[i]-48;
      hx[i/2]= tmp2*16 + tmp1;
    }
  }

}

