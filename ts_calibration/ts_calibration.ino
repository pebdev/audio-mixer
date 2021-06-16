/*************************************************************************************************/
/*  Project : TouchScreen calibration                                                            */
/*  Author  : pierre.emmanuel.balandreau@gmail.com                                               */
/*  Date    : 12.11.2020                                                                         */
/*************************************************************************************************/

/* I N C L U D E S *******************************************************************************/
#include <SPI.h>
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>


/* D E F I N E S *********************************************************************************/
/* Screen  resolution */
#define SCREEN_HRES           (tft.width())
#define SCREEN_VRES           (240) //tft.height())

/* TFT */
#define TFT_DC                (7)
#define TFT_CS                (6)
#define TFT_RST               (10)  /* Connected to 3V3 in real, but needed by the library */

/* CS */
#define TS_CS                 (3)
#define TS_IRQ                (2)

/* Colors */
#define TFT_WHITE             ILI9341_WHITE
#define TFT_BLACK             ILI9341_BLACK
#define TFT_RED               ILI9341_RED
#define TFT_GREEN             ILI9341_GREEN


/* S T R U C T U R E S ***************************************************************************/
typedef struct {
  uint16_t x = 0;
  uint16_t y = 0;
} sCoord;


/* D E C L A R A T I O N S ***********************************************************************/
XPT2046_Touchscreen ts(TS_CS, TS_IRQ);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);


/* F U N C T I O N S  T O  E X P O R T ***********************************************************/
/* Copy and past this code into your ino file to manage touch screen calibration.                */
double ts_xCoef, ts_yCoef, ts_xShift, ts_yShift = 0.0;

void ts_calibInit (double xCoef, double yCoef, double xShift, double yShift)
{
  ts_xCoef = xCoef;
  ts_yCoef = yCoef;
  ts_xShift = xShift;
  ts_yShift = yShift;
}

uint16_t ts_getCalibratedCoordX (uint16_t xReceived)
{
  double retval = (xReceived/ts_xCoef)-ts_xShift;
  return (uint16_t)(retval);
}

uint16_t ts_getCalibratedCoordY (uint16_t yReceived)
{
  double retval = (yReceived/ts_yCoef)-ts_yShift;
  return (uint16_t)(retval);
}
/*************************************************************************************************/


/* F U N C T I O N S *****************************************************************************/
void setup (void)
{
  Serial.begin (9600);

  /* Hardware initilization */
  SPI.begin();
  tft.begin ();
  ts.begin();

  /* Settings */
  tft.setRotation(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  ts.setRotation(1);
}

/*-----------------------------------------------------------------------------------------------*/
void loop()
{
  sCoord coord[4];
  
  /* Entry point */
  drawCross(SCREEN_HRES/2, SCREEN_VRES/2, TFT_GREEN);
  getTsCoords (NULL);
  drawCross(SCREEN_HRES/2, SCREEN_VRES/2, TFT_BLACK);

  /* point #1-#4 */
  drawCross(30, 30, TFT_RED);
  getTsCoords (&coord[0]);
  drawCross(30, 30, TFT_BLACK);
  
  drawCross(SCREEN_HRES-30, 30, TFT_RED);
  getTsCoords (&coord[1]);
  drawCross(SCREEN_HRES-30, 30, TFT_BLACK);

  drawCross(SCREEN_HRES-30, SCREEN_VRES-30, TFT_RED);
  getTsCoords (&coord[2]);
  drawCross(SCREEN_HRES-30, SCREEN_VRES-30, TFT_BLACK);

  drawCross(30, SCREEN_VRES-30, TFT_RED);
  getTsCoords (&coord[3]);
  drawCross(30, SCREEN_VRES-30, TFT_BLACK);

  /* Calculate calibration coeficients and shift */
  double xPixels = SCREEN_HRES-60;
  double yPixels = SCREEN_VRES-60;
  double xTs1   = coord[1].x - coord[0].x;
  double xTs2   = coord[2].x - coord[3].x;
  double yTs1   = coord[3].y - coord[0].y;
  double yTs2   = coord[2].y - coord[1].y;
  double xDist  = (xTs1 + xTs2)/2;
  double yDist  = (yTs1 + yTs2)/2;
  double xCoef = xDist/xPixels;
  double yCoef = yDist/yPixels;
  double xShift = (((coord[0].x+coord[3].x)/2)/xCoef)-30;
  double yShift = (((coord[0].y+coord[1].y)/2)/yCoef)-30;

  Serial.println();
  Serial.print("Call this function in your code :");
  Serial.print("   ts_calibInit (");
  _float(xCoef,4);
  Serial.print(",");
  _float(yCoef,4);
  Serial.print(",");
  _float(xShift,4);
  Serial.print(",");
  _float(yShift,4);
  Serial.println(")");
  delay(2000);

  sCoord prevCoord;
  ts_calibInit (xCoef, yCoef, xShift, yShift);
  
  while (1)
  {
    delay(10);
    while (ts.touched());
    TS_Point p = ts.getPoint();
    drawCross(ts_getCalibratedCoordX(prevCoord.x), ts_getCalibratedCoordY(prevCoord.y), TFT_BLACK);
    drawCross(ts_getCalibratedCoordX(p.x), ts_getCalibratedCoordY(p.y), TFT_GREEN);
    prevCoord.x = p.x;
    prevCoord.y = p.y;
  }
}

/*-----------------------------------------------------------------------------------------------*/
void _float(double number, uint8_t digits)
{
  // Handle negative numbers
  if (number < 0.0)
  {
     Serial.print('-');
     number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i)
    rounding /= 10.0;
 
  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  Serial.print(int_part);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
    Serial.print(".");

  // Extract digits from the remainder one at a time
  while (digits-- > 0)
  {
    remainder *= 10.0;
    int toPrint = int(remainder);
    Serial.print(toPrint);
    remainder -= toPrint;
  }
}

/*-----------------------------------------------------------------------------------------------*/
void drawCross (int x, int y, unsigned int color)
{
  tft.drawLine(x - 5, y, x + 5, y, color);
  tft.drawLine(x, y - 5, x, y + 5, color);
}

/*-----------------------------------------------------------------------------------------------*/
void getTsCoords (sCoord* coord)
{
  coord->x = 0;
  coord->y = 0;
  
  while (!ts.touched());
  while (ts.touched())
  {
    TS_Point p = ts.getPoint();
    
    if (coord != NULL)
    {
      if ((coord->x != 0) && (coord->y != 0))
      {
        coord->x += p.x;
        coord->y += p.y;
        coord->x /= 2;
        coord->y /= 2;
      }
      else
      {
        coord->x = p.x;
        coord->y = p.y;
      }

      Serial.print("X=");
      Serial.print(coord->x);
      Serial.print(" / Y=");
      Serial.println(coord->y);
    }
  }
  
  delay(100);
}
