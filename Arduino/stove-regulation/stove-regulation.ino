
// Firewood regulation
// Author : Philippe Collignon (philippe.collignon@email.com)
// CC Attribution Non-Commercial ShareAlike License.
// WARNING : always let a minimum draft.
// Not providing enough air supply to stove is dangerous.  It can produce fatal CO gaz or incomplete burned gaz explosion.
// Disclaimer : I am not responsible of any use of this script. By using this script you take all responsability of its configuration and usage.

#include <max6675.h>
#include <Servo.h>
#include <LiquidCrystal.h>
#include <math.h>

// Thermocouple variables
int thermoCsPort = 6;   // CS pin on MAX6675
int thermoSoPort = 7;   // SO pin of MAX6675
int thermoSckkPort = 8; // SCK pin of MAX6675
int units = 1;          // Units to readout temp (0 = ˚F, 1 = ˚C)
float error = 0.0;      // Temperature compensation error

// Buzzer variables
int buzzerPort = 13;              // Buzzer port
int buzzerRefillFrequency = 2000; // Buzzer tone frequency for refill alarm
int buzzerRefillRepeat = 3;       // Number of refill alarm tones
int buzzerRefillDelay = 1000;     // Delay of refill alarm tone
int buzzerCloseFrequency = 1000;  // Buzzer tone frequency for end of fire valve close alarm
int buzzerCloseRepeat = 2;        // Number of tones for end of fire valve close alarm
int buzzerCloseDelay = 2000;      // Delay of tone for end of fire valve close alarm

// Reset button variables
int resetPort = 9;

// Potentiometre variables
int servoPort = 0;
int potentioPort = 0;

// LCD Ports variables
int lcdPort1 = 12;
int lcdPort2 = 11;
int lcdPort3 = 5;
int lcdPort4 = 4;
int lcdPort5 = 3;
int lcdPort6 = 2;

// Device objects
Servo myservo;
int angleCalibration = 95;                                        // modify this value so that initial angle of the servo is 0°
float servoCalibration = 1;                                       // modify this value so that servo is at 90° when angle is 90°
MAX6675 thermocouple(thermoSckkPort, thermoCsPort, thermoSoPort); //,units,error);
LiquidCrystal lcd(lcdPort1, lcdPort2, lcdPort3, lcdPort4, lcdPort5, lcdPort6);

// Regulation variables (Base on PID :
// draft = kP* errP + kI * errI + kD * errD
// errP = consigneTemperature - temperature
// errI = errP integral
// errD = errP derivative
int temperature = 0;
float temperatureMin = 50;         // under this temperature, the regulation starts an integral measure to estimate end or fire and close the valve
float consigneTemperature = 130.0; // the desired temperature measured by the thermocouple
float errP = 0.0;
float errD = 0.0;
float errI = 0.0;
float errOld = 0.0;
float kP = 5;       // P parameter of the PID regulation
float kI = 0.0005;  // I parameter of the PID regulation
float kD = 0.00005; // D parameter of the PID regulation

float refillTrigger = 7500; // refillTrigger used to notify need of a wood refill
float closeTrigger = 15000; // closeTrigger used to close vlave at end of combustion

int potentio = 0;
int oldPotentio = 0;
float potentioMax = 1023.0; // Potentiometre calibration.
int potentioRelMax = 80;    // Potentiometre value above which the regulator runs in automatic mode

int reset = HIGH;

int angle = 0;
int draft = 0;
int oldDraft = 0;
float maxDraft = 100.0;
float minDraft = 5.0;
float zeroDraft = 0.0;

String messageTmp;
String messageDrf;

boolean closeBuzzer = true;
boolean refillBuzzer = true;

long debounceTime = 0;
long debounceDelay = 50;
int lastResetState = LOW;
int resetState = LOW;

void setup()
{
  Serial.begin(9600);
  myservo.attach(10);
  myservo.write(0);
  myservo.detach();
  lcd.begin(16, 2);
  pinMode(buzzerPort, OUTPUT);
  pinMode(resetPort, INPUT);
}

void loop()
{

  temperature = thermocouple.readCelsius() - 60; //-32)*5/9;
  potentio = analogRead(potentioPort);
  reset = digitalRead(resetPort);

  delay(500);
  if (temperature == -1)
  {
    Serial.println("Thermocouple Error!!"); // Temperature is -1 and there is a thermocouple error
  }
  else
  {
   

    // Read potentiometer
    potentio = (potentioMax - potentio) * 100 / potentioMax;

    if ((potentio < potentioRelMax))
    {
      // Potentiometre regulation
      draft = round(potentio * maxDraft / 100);
      errI = 0;
      errD = 0;
      closeBuzzer = true;
      refillBuzzer = true;

      messageDrf = "Drf=" + String(round(draft / maxDraft * 100)) + "%" + " <)= " + String(round(draft * 90 / maxDraft)) + "";
    }
    else
    {
      if (errI < closeTrigger)
      {
        // PID regulation
        errP = consigneTemperature - temperature;
        errI = errI + errP;
        errD = errP - errOld;
        draft = kP * errP + kI * errI + kD * errD;
        errOld = errP;

        // Limit values to physical constraints ..
        if (draft < minDraft)
          draft = minDraft;
        if (draft > maxDraft)
          draft = maxDraft;

        // Close end fire
        if (errI >= closeTrigger)
          errI = closeTrigger;
        if (temperature < temperatureMin)
          errI = 0;
        if (errI >= closeTrigger)
          draft = zeroDraft;

        messageDrf = "Drf=" + String(round(draft / maxDraft * 100)) + "%" + " <)= " + String(round(draft * 90 / maxDraft)) + "";
        // Refill Alarm
        if (errI > refillTrigger)
        {
          messageDrf = "Please refill !!";
          if (refillBuzzer)
          {
            for (int i = 0; i < buzzerRefillRepeat; i++)
            {
              tone(buzzerPort, buzzerRefillFrequency);
              delay(buzzerRefillDelay);
              noTone(buzzerPort);
              delay(buzzerRefillDelay);
            }
            refillBuzzer = false;
          }
          if (temperature > consigneTemperature)
          {
            errI = 0;
            errD = 0;
            refillBuzzer = true;
          }
        }
      }

      else
      {
        // Close Valve if end of combustion
        messageDrf = "Fire End.  <)= " + String(round(draft * 90 / maxDraft)) + "";

        if (closeBuzzer)
        {
          for (int i = 0; i < buzzerCloseRepeat; i++)
          {
            tone(buzzerPort, buzzerCloseFrequency);
            delay(buzzerCloseDelay);
            noTone(buzzerPort);
            delay(buzzerCloseDelay);
          }
          closeBuzzer = false;
        }
      }
    }

     // Handle reset button with debounce support
   
        if (reset == HIGH)
        {
          draft = 100;
          errI = 0;
          errD = 0;
          closeBuzzer = true;
          refillBuzzer = true;
          messageDrf = "Reset ... ";
          delay(1000);
        }
     

    // Display message on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    messageTmp = "Tmp=" + String(temperature) + "C" + " Po=" + round(potentio) + "%";
    lcd.print(messageTmp);
    lcd.setCursor(0, 1);
    lcd.print(messageDrf);
    // Serial Plotter
    // Serial.print(round(draft/maxDraft* 100));
    // Serial.print(" ");
    // Serial.println(temperature);

    // Turn servo only for 5° delta
    if (abs(oldDraft - draft) > 5)
    {
      lcd.print(" x");
      delay(500);
      myservo.attach(10);
      angle = angleCalibration + (draft * 90 / maxDraft * servoCalibration);
      myservo.write(angle);
      delay(500);
      myservo.detach();

      oldPotentio = potentio;
      oldDraft = draft;
      
    }
    delay(1500);
  }
}
