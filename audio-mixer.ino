/*************************************************************************************************/
/*  Project : Audio Mixer                                                                        */
/*  Author  : PEB <pebdev@lavache.com>                                                           */
/*  Date    : 07.02.2021                                                                         */
/*************************************************************************************************/
/*  This program is free software: you can redistribute it and/or modify                         */
/*  it under the terms of the GNU General Public License as published by                         */
/*  the Free Software Foundation, either version 3 of the License, or                            */
/*  (at your option) any later version.                                                          */
/*                                                                                               */
/*  This program is distributed in the hope that it will be useful,                              */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                                */
/*  GNU General Public License for more details.                                                 */
/*                                                                                               */
/*  You should have received a copy of the GNU General Public License                            */
/*  along with this program.  If not, see <https://www.gnu.org/licenses/>.                       */
/*************************************************************************************************/
/* A:app1:app2:app3                                                                              */
/* O:out1:out2                                                                                   */
/*************************************************************************************************/


/* I N C L U D E S *******************************************************************************/
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>


/* D E F I N E S *********************************************************************************/
/* Sleep */
#define SLEEP_TIME_SEC        (60*5)

/* GPIO */
#define IO_IN_ANALOG_M        (A0)
#define IO_IN_ANALOG_S1       (A1)
#define IO_IN_ANALOG_S2       (A2)
#define IO_IN_ANALOG_S3       (A3)
#define IO_IN_ANALOG_S4       (A4)
#define IO_IN_OK              (4)
#define IO_IN_CANCEL          (5)
#define IO_OUT_OK_LED         (9)
#define IO_OUT_CANCEL_LED     (8)
#define IO_OUT_LCD            (A5)

/* TFT */
#define TFT_DC                (7)
#define TFT_CS                (6)
#define TFT_RST               (10)  /* Connected to 3V3 in real, but needed by the library */

/* CS */
#define TS_CS                 (3)
#define TS_IRQ                (2)

/* Static configuration */
#define NB_MAX_APPLICATIONS   (4)
#define NB_MAX_OUTPUTS        (4)

/* GUI */
#define SCREEN_WIDTH          tft.width()
#define SCREEN_HEIGHT         tft.height()

/* GUI : applications layout */
#define APP_LAYOUT_XMIN       (5)
#define APP_LAYOUT_YMIN       (35)
#define APP_LAYOUT_XMAX       ((SCREEN_WIDTH/2)-2)
#define APP_LAYOUT_YMAX       (235)

/* GUI : outputs layout */
#define OUT_LAYOUT_XMIN       ((SCREEN_WIDTH/2)+2)
#define OUT_LAYOUT_YMIN       (APP_LAYOUT_YMIN)
#define OUT_LAYOUT_XMAX       (SCREEN_WIDTH-5)
#define OUT_LAYOUT_YMAX       (APP_LAYOUT_YMAX)

/* GUI : buttons */
#define ALL_BUTTON_SPACE      (5)

#define APP_BUTTON_XMIN       (APP_LAYOUT_XMIN+ALL_BUTTON_SPACE)
#define APP_BUTTON_XMAX       (APP_LAYOUT_XMAX-ALL_BUTTON_SPACE)
#define APP_BUTTON_YMIN       (APP_LAYOUT_YMIN+(ALL_BUTTON_SPACE*2))
#define APP_BUTTON_YSIZE      ((APP_LAYOUT_YMAX-APP_LAYOUT_YMIN-(ALL_BUTTON_SPACE*2)-((mixer.nbApplications-1)*(ALL_BUTTON_SPACE+2)))/mixer.nbApplications)

#define OUT_BUTTON_XMIN       (OUT_LAYOUT_XMIN+ALL_BUTTON_SPACE)
#define OUT_BUTTON_XMAX       (OUT_LAYOUT_XMAX-ALL_BUTTON_SPACE)
#define OUT_BUTTON_YMIN       (OUT_LAYOUT_YMIN+(ALL_BUTTON_SPACE*2))
#define OUT_BUTTON_YSIZE      ((OUT_LAYOUT_YMAX-OUT_LAYOUT_YMIN-(ALL_BUTTON_SPACE*2)-((mixer.nbOutputs-1)*(ALL_BUTTON_SPACE+2)))/mixer.nbOutputs)

#define TYPE_BUTTON_APP       (1)
#define TYPE_BUTTON_OUT       (2)

/* Deej */
#define DEEJ_NUM_SLIDERS      (5)
#define DEEJ_ANALOG_SENSI     (2000)


/* S T R U C T U R E S ***************************************************************************/
typedef struct {
  uint16_t x;
  uint16_t y;
} sCoord;

typedef struct {
  uint8_t id = 0;
  String  appname = "";
} appParameters;

typedef struct {
  uint8_t id = 0;
  String  outname = "";
} outputParameters;

typedef struct {
  uint8_t nbApplications;
  uint8_t nbOutputs;
  int8_t selectedApp;
  int8_t selectedOut;
  appParameters appList[NB_MAX_APPLICATIONS];
  outputParameters outList[NB_MAX_OUTPUTS];  
} mixerParameters;


/* D E C L A R A T I O N S ***********************************************************************/
XPT2046_Touchscreen ts(TS_CS, TS_IRQ);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);
mixerParameters mixer;

/* Touchscreen calibrations */
double ts_xCoef, ts_yCoef, ts_xShift, ts_yShift = 0.0;

/* Sleep mode */
bool sleepModeStatus;
uint32_t sleepStartTime;

/* deej */
uint16_t analogSliderValues[DEEJ_NUM_SLIDERS];
const int analogInputs[DEEJ_NUM_SLIDERS] = {IO_IN_ANALOG_M, IO_IN_ANALOG_S1, IO_IN_ANALOG_S2, IO_IN_ANALOG_S3, IO_IN_ANALOG_S4};


/* S T D  F U N C T I O N S ***********************************************************************/
String std_substring (String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length();
  
  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


/* I O  F U N C T I O N S *************************************************************************/
bool io_buttonIsPushed (uint8_t id)
{
  return digitalRead(id);
}

/*-----------------------------------------------------------------------------------------------*/
uint8_t io_update (void)
{
  uint8_t retval = 0;
  uint8_t ioOk = io_buttonIsPushed (IO_IN_OK);
  uint8_t ioCancel = io_buttonIsPushed (IO_IN_CANCEL);
  
  if ((ioCancel == HIGH) || (ioOk == HIGH))
  {
    if (sleepModeStatus == false)
    {
      if (ioCancel == HIGH)
      {
        /* Cancel action */
        retval = 1;
      }
      else if (ioOk == HIGH)
      {
        /* Send data to the PC */
        if ((mixer.selectedApp != -1) && (mixer.selectedOut != -1))
        {
          retval = 2;
        }
      }
      
      /* Refresh GUI */
      if ((retval == 1) || (retval == 2))
      {
        drawButton (mixer.selectedApp, TYPE_BUTTON_APP, false);
        drawButton (mixer.selectedOut, TYPE_BUTTON_OUT, false);
      }
    }
    else
    {
      if (ioCancel == HIGH)
      {
        /* Wake up */
        retval = 3;
      }
    }
  }

  return retval;
}

/*-----------------------------------------------------------------------------------------------*/
void led_update (void)
{
  if ((mixer.selectedApp != -1) && (mixer.selectedOut != -1))
  {
    digitalWrite(IO_OUT_OK_LED, HIGH);
  }
  else
  {
    digitalWrite(IO_OUT_OK_LED, LOW);
  }
}


/* U A R T  F U N C T I O N S *********************************************************************/
void serial_update (void)
{
  if (Serial.available() > 0)
  {
    String ret = Serial.readString();
    deej_parseIncomingCmd (ret);
  }
}


/* D E E J  F U N C T I O N S *********************************************************************/
int8_t deej_updateSliderValues (void)
{
  int8_t retval = 0;  /* 0 no update | 1 update to send */
  
  for (int i=0; i<DEEJ_NUM_SLIDERS; i++)
  {
    uint16_t value = analogRead(analogInputs[i]);
    if ((value > (analogSliderValues[i]+DEEJ_ANALOG_SENSI)) && (value < (analogSliderValues[i]-DEEJ_ANALOG_SENSI)))
    {
      retval = 1;
    }
    analogSliderValues[i] = value;
  }

  return retval;
}

/*-----------------------------------------------------------------------------------------------*/
void deej_sendSliderValues (void)
{
  String builtString = String("S:");

  for (uint8_t i=0; i<DEEJ_NUM_SLIDERS; i++)
  {
    builtString += String((int)analogSliderValues[i]);

    if (i < (DEEJ_NUM_SLIDERS-1))
    {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
}

/*-----------------------------------------------------------------------------------------------*/
void deej_sendAppConfiguration (void)
{
  if ((mixer.selectedApp != -1) && (mixer.selectedOut != -1))
  {
    String builtString = String("A:");
    builtString += mixer.appList[mixer.selectedApp].appname;
    builtString += String("|");
    builtString += mixer.outList[mixer.selectedOut].outname;
    
    Serial.println(builtString);
  }
}

/*-----------------------------------------------------------------------------------------------*/
void deej_parseIncomingCmd (String message)
{
  bool updated = false;
  
  if ((message.charAt(0) == 'A') && (message.charAt(1) == ':'))
  {
    updated = true;
    mixer.nbApplications = 0;
    
    for (uint8_t i=0; i<NB_MAX_APPLICATIONS;i++)
    {
      String ret = std_substring (message, ':', i+1);
      if (ret  != "")
      {
        mixer.appList[i].appname = ret;
        mixer.appList[i].id = i;
        mixer.nbApplications++;
      }
      else
      {
        break;
      }
    }
  }
  else if ((message.charAt(0) == 'O') && (message.charAt(1) == ':'))
  {
    updated = true;
    mixer.nbOutputs = 0;
    
    for (uint8_t i=0; i<NB_MAX_OUTPUTS;i++)
    {
      String ret = std_substring (message, ':', i+1);
      if (ret  != "")
      {
        mixer.outList[i].outname = ret;
        mixer.outList[i].id = i;
        mixer.nbOutputs++;
      }
      else
      {
        break;
      }
    }
  }

  /* Refresh screen */
  if (updated == true)
  {
    show ();
  }
}


/* T S  F U N C T I O N S ************************************************************************/
void ts_calibInit (double xCoef, double yCoef, double xShift, double yShift)
{
  ts_xCoef  = xCoef;
  ts_yCoef  = yCoef;
  ts_xShift = xShift;
  ts_yShift = yShift;
}

/*-----------------------------------------------------------------------------------------------*/
uint16_t ts_getCalibratedCoordX (uint16_t xReceived)
{
  double retval = (xReceived/ts_xCoef)-ts_xShift;
  return (uint16_t)(retval);
}

/*-----------------------------------------------------------------------------------------------*/
uint16_t ts_getCalibratedCoordY (uint16_t yReceived)
{
  double retval = (yReceived/ts_yCoef)-ts_yShift;
  return (uint16_t)(retval);
}

/*-----------------------------------------------------------------------------------------------*/
bool ts_update (void)
{
  bool retval = false;
  
  /* Update GUI according touchscreen interaction */
  if (ts.touched())
  {
    if (sleepModeStatus == false)
    {
      TS_Point p = ts.getPoint();
  
      sCoord coord;
      coord.x = ts_getCalibratedCoordX(p.x);
      coord.y = ts_getCalibratedCoordY(p.y);
  
      int8_t idButtonApp = gui_buttonIsPushed (coord.x, coord.y, TYPE_BUTTON_APP);
      int8_t idButtonOut = gui_buttonIsPushed (coord.x, coord.y, TYPE_BUTTON_OUT);
  
      if (idButtonApp != -1)
      {
        drawButton (mixer.selectedApp, TYPE_BUTTON_APP, false);
        drawButton (idButtonApp, TYPE_BUTTON_APP, true);
        mixer.selectedApp = idButtonApp;
      }
  
      if (idButtonOut != -1)
      {
        drawButton (mixer.selectedOut, TYPE_BUTTON_OUT, false);
        drawButton (idButtonOut, TYPE_BUTTON_OUT, true);
        mixer.selectedOut = idButtonOut;
      }
    }

    retval = true;
  }
  
  return retval; 
}


/* S L E E P  F U N C T I O N S ******************************************************************/
void sleep_reset (void)
{
  sleepStartTime  = millis();
  sleepModeStatus = false;
}

/*-----------------------------------------------------------------------------------------------*/
void sleep_update (void)
{
  /* Check time */
  if (((millis()-sleepStartTime)/1000) > SLEEP_TIME_SEC)
  {
    digitalWrite(IO_OUT_LCD, LOW);
    sleepModeStatus = true;
  }
  else
  {
    digitalWrite(IO_OUT_LCD, HIGH);
    sleepModeStatus = false;
  }
}


/* D R A W I N G  F U N C T I O N S **************************************************************/
void drawFilledRect (uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax, uint16_t color, uint16_t colorBorders)
{
  tft.fillRect (xmin, ymin, xmax-xmin, ymax-ymin, color);
  tft.drawRect (xmin, ymin, xmax-xmin, ymax-ymin, colorBorders);
  yield ();
}

/*-----------------------------------------------------------------------------------------------*/
void drawRoundRect (uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax, uint16_t color)
{
  tft.drawRoundRect (xmin, ymin, xmax-xmin, ymax-ymin, 5, color);
  yield ();
}

/*-----------------------------------------------------------------------------------------------*/
void drawFilledCircle (uint16_t x, uint16_t y, uint16_t radius, uint16_t color, uint16_t colorBorder)
{
  tft.fillCircle (x, y, radius, color);
  tft.drawCircle (x, y, radius, colorBorder);
  yield ();
}

/*-----------------------------------------------------------------------------------------------*/
void drawText (uint16_t x, uint16_t y, uint16_t color, int16_t bgcolor, uint16_t tSize, String text)
{
  tft.setCursor (x, y);

  if (bgcolor >= 0)
    tft.setTextColor (color, bgcolor);
  else
    tft.setTextColor (color);
    
  tft.setTextSize (tSize);
  tft.println (text);
}


/* H M I  F U N C T I O N S **********************************************************************/
void drawButton (uint8_t id, uint8_t type, bool selected)
{
  uint16_t color = selected==true?ILI9341_ORANGE:ILI9341_NAVY;
  uint16_t textColor = selected==true?ILI9341_BLACK:ILI9341_LIGHTGREY;
  
  if (type == TYPE_BUTTON_APP)
  {
    uint16_t ymin = APP_BUTTON_YMIN+(id*(APP_BUTTON_YSIZE + ALL_BUTTON_SPACE));
    drawFilledRect (APP_BUTTON_XMIN, ymin, APP_BUTTON_XMAX, ymin+APP_BUTTON_YSIZE, color , ILI9341_NAVY );
    drawText (APP_BUTTON_XMIN+3, ymin+15, textColor, -1, 2, mixer.appList[id].appname);
  }
  else
  {
    uint16_t ymin = OUT_BUTTON_YMIN+(id*(OUT_BUTTON_YSIZE + ALL_BUTTON_SPACE));
    drawFilledRect (OUT_BUTTON_XMIN, ymin, OUT_BUTTON_XMAX, ymin+OUT_BUTTON_YSIZE, color , ILI9341_NAVY );
    drawText (OUT_BUTTON_XMIN+3, ymin+15, textColor, -1, 2, mixer.outList[id].outname);
  }
}

/*-----------------------------------------------------------------------------------------------*/
void show ()
{
  /* Header */
  tft.fillScreen (ILI9341_BLACK);
  drawText (4, 1, ILI9341_BLACK, ILI9341_BLUE, 2, "        Audio Mixer       ");

  /* Layouts */
  drawRoundRect (APP_LAYOUT_XMIN, APP_LAYOUT_YMIN, APP_LAYOUT_XMAX, APP_LAYOUT_YMAX, ILI9341_LIGHTGREY);
  drawText (APP_LAYOUT_XMIN+15, APP_LAYOUT_YMIN-9, ILI9341_WHITE, ILI9341_BLACK, 2, " Apps ");

  drawRoundRect (OUT_LAYOUT_XMIN, OUT_LAYOUT_YMIN, OUT_LAYOUT_XMAX, OUT_LAYOUT_YMAX, ILI9341_LIGHTGREY);
  drawText (OUT_LAYOUT_XMIN+15, OUT_LAYOUT_YMIN-9, ILI9341_WHITE, ILI9341_BLACK, 2, " Outputs ");
  
  /* Applications */
  for (uint8_t i=0; i<mixer.nbApplications; i++)
  {
    drawButton (i, TYPE_BUTTON_APP, false);
  }

  /* Outputs */
  for (uint8_t i=0; i<mixer.nbOutputs; i++)
  {
    drawButton (i, TYPE_BUTTON_OUT, false);
  }
}

/*-----------------------------------------------------------------------------------------------*/
int8_t gui_buttonIsPushed (uint16_t x, uint16_t y, uint8_t type)
{
  /* Applications buttons */
  if (type == TYPE_BUTTON_APP)
  {
    for (uint8_t i=0; i<mixer.nbApplications; i++)
    {
      uint16_t xmin = APP_BUTTON_XMIN;
      uint16_t xmax = APP_BUTTON_XMAX;
      uint16_t ymin = APP_BUTTON_YMIN+(i*(APP_BUTTON_YSIZE + ALL_BUTTON_SPACE));
      uint16_t ymax = ymin+APP_BUTTON_YSIZE;
  
      if ((x>=xmin) && (x<=xmax) && (y>=ymin) && (y<=ymax))
      {
        return mixer.appList[i].id;
      }
    }
  }

  /* Outputs button */
  else
  {
    for (uint8_t i=0; i<mixer.nbOutputs; i++)
    {
      uint16_t xmin = OUT_BUTTON_XMIN;
      uint16_t xmax = OUT_BUTTON_XMAX;
      uint16_t ymin = OUT_BUTTON_YMIN+(i*(OUT_BUTTON_YSIZE + ALL_BUTTON_SPACE));
      uint16_t ymax = ymin+OUT_BUTTON_YSIZE;
  
      if ((x>=xmin) && (x<=xmax) && (y>=ymin) && (y<=ymax))
      {
        return mixer.outList[i].id;
      }
    }
  }

  return -1;
}

/*-----------------------------------------------------------------------------------------------*/
void setup (void)
{
  Serial.begin (9600);

  /* Hardware initilization */
  SPI.begin();
  tft.begin ();
  ts.begin();

  pinMode(IO_IN_OK, INPUT);
  pinMode(IO_IN_CANCEL, INPUT);

  for (uint8_t i=0; i<DEEJ_NUM_SLIDERS; i++)
  {
    pinMode(analogInputs[i], INPUT);
    analogSliderValues[i] = 0;
  }
  
  pinMode(IO_OUT_CANCEL_LED, OUTPUT);
  pinMode(IO_OUT_OK_LED, OUTPUT);
  pinMode(IO_OUT_LCD, OUTPUT);
  
  digitalWrite(IO_OUT_CANCEL_LED, HIGH);
  digitalWrite(IO_OUT_OK_LED, LOW);
  digitalWrite(IO_OUT_LCD, HIGH);
  
  /* Screen orientation  */
  tft.setRotation (1);
  ts.setRotation(1);
  ts_calibInit (10.7308,14.7583,29.0824,20.4122);

  /* Initilization */
  mixer.nbApplications  = 0;
  mixer.nbOutputs       = 0;
  mixer.selectedApp = -1;
  mixer.selectedOut = -1;

  /* Sleep mode */
  sleepModeStatus = false;
  sleepStartTime = millis();

  /* Display GUI */
  show ();
}

/*-----------------------------------------------------------------------------------------------*/
void loop(void)
{
  bool sendAppConfig = false;
  uint8_t retval = 0;

  /* Check update of ts modules */
  if (ts_update () == true)
  {
    sleep_reset ();
    delay(150);
  }

  /* Check update of IO modules */
  retval = io_update();
  if (retval != 0)
  {
    if (retval == 2)
    {
      sendAppConfig = true;
    }
    else
    {
      /* Reset variables */
      mixer.selectedApp = -1;
      mixer.selectedOut = -1;
    }
    sleep_reset ();
  }

  /* Send data to the PC */
  if (deej_updateSliderValues() == 1)
  {
    deej_sendSliderValues ();
  }

  if (sendAppConfig == true)
  {
    deej_sendAppConfiguration ();

    /* Reset variables */
    mixer.selectedApp = -1;
    mixer.selectedOut = -1;
  }

  /* Update led status */
  led_update ();

  /* Update the sleep mechanism */
  sleep_update ();

  /* Update serial data */
  serial_update ();
  
  delay(10);
}
