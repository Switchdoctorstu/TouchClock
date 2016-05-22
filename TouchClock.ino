// include the library code:

#include <Arduino.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <SWTFT.h> // Hardware-specific library
#include <TouchScreen.h>

#include <Time.h>
//#include <TimeLib.h>
//#include <Wire.h>
//#include <DS1307RTC.h>  // RTC

#include <String.h>

// watchdog
//#include <avr/wdt.h>


/*

// Stuarts code to drive TFT display 
*/
//****************************************************
// define the modes that we can work as
#define RTCMODE 0  // running off RTC
#define GPSMODE 1  // running from GPS
#define SYSMODE 3   // running off system clock
#define NUMMODES 3
#define DEBUGWATCHDOG false
#define DEBUGREPORT true
#define DEBUGTIME false
#define DEBUGTOUCH true
#define DEBUGBUTTONS true

#define MAXI2CDEVICES 10

#define YP A1  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 7   // can be a digital pin
#define XP 6   // can be a digital pin

#define BRIGHTNESS 64 // set max brightness
// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define MINPRESSURE 10
#define MAXPRESSURE 1000

#define BOXSIZE 40
#define PENRADIUS 3


// ************************************
// Define Objects and variables
// ************************************

SWTFT tft;
tmElements_t tm;
time_t timeNow;
DateTime dt; 
// date storage data type
struct{
	int mill; // millenea offset
	int year;
	int month;
	int day;
	int hour;
	int min;
} StuTime

// bank of stored dates

StuTime myDates[20];

// resistance between X+ and X-  300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

int lastsecond; // holds the prevoius second

// screen calibration
int TS_MINX = 220;
int TS_MINY =180;
int TS_MAXX =850;
int TS_MAXY =900;
int xoffset=10;
int yoffset=-12;

// int i2cdelay=5;//wait after each packet
int oldcolor, currentcolor;
TSPoint touched; // last valid point
TSPoint buttonSize; // dimensions of button x,y
// Declare all the global vars
unsigned long waitcheckTime=0; // timer for time checking
unsigned long waitcheckButtons=0; // timer for buttons
unsigned long intervalcheckTime=1012;
unsigned long intervalcheckButtons=503;
unsigned long reportTime=1007;
unsigned long reportInterval=10007;
unsigned long lasttime=0;
unsigned long thistime=0; // Time handles
unsigned long millisNow=0; // used for checking internal loop timers
unsigned long NextTimeSyncTime=0;
unsigned long NextTimeSyncInterval=60023; // 30 seconds
int screenWidth =0;
int screenHeight = 0;
int screenRotation = 0;
int buttonCount=6;
int menuState=0; // which menu we're in
const char *monthName[12] = {// array of month names
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const int daysInMonth[12]={ // array of days in month
	31,28,31,30,31,30,
	31,31,30,31,30,31};
// int argcount=0;  // number of arguments
// int returncode=0; // generic return code

/************************************************************************************/
/* CODE SEGMENT */



void setup(void) {
  Serial.begin(9600);
  Serial.println(F("Touch Clock!"));
  
  tft.reset();
  
  uint16_t identifier = tft.readID();

  Serial.print(F("LCD driver chip: "));
  Serial.println(identifier, HEX);
    
  tft.begin(identifier);
 
	screenRotation=1;
	tft.setRotation(screenRotation); 
	tft.fillScreen(BLACK);
	screenHeight=tft.height();
	screenWidth=tft.width();
	
	currentcolor = RED;
	clockSetup();   // setup clock
	pinMode(13, OUTPUT);
 
 buttonCount=6;
	// define button sizes
if(screenRotation==0){
  buttonSize.y=screenWidth/buttonCount;
  buttonSize.x=screenHeight/buttonCount;
}else{
	buttonSize.x=screenWidth/buttonCount;
	buttonSize.y=screenHeight/buttonCount;
}
	showMenu();  // draw the buttons
}

void loop(){
	doTouch();
    gettime();   // read the time from the RTC or GPS 
    
//    checkButtons();
//    if(DEBUGREPORT)checkReport(); // see if we need to report
//    checktimesync(); // keep RTC in sync with GPS if available
 showClock();    
}
void showMenu(){
	// draws menu on screen
	
	int16_t colour=0;

	int bx=buttonSize.x;
	int by=	buttonSize.y;
	for(int i=0;i<6;i++){ 
	  delay(200); 
	  colour=(B11111110<<(i*2));
	  tft.fillRect(i*bx+1,1, bx-2, by-2, colour);
	}
 tft.setCursor(07, 05);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("NOW  SET DEL EDIT NEW LIST");
  
  
   
	
}

void doTouch(){
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  TSPoint tp =p; // temporary point 
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!

	if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
		if(DEBUGTOUCH){
			Serial.print("Initial X = "); Serial.print(p.x);
			Serial.print("\tY = "); Serial.print(p.y);
			Serial.print("\tPressure = "); Serial.println(p.z);
		}
		
	  // recalibrate
    if(p.x>TS_MAXX)TS_MAXX=p.x;
    if(p.y>TS_MAXY)TS_MAXY=p.y;
    if(p.x<TS_MINX)TS_MINX=p.x;
    if(p.y<TS_MINY)TS_MINY=p.y;
  
		// handle screen rotation
		
		if(screenRotation==1){
		// tp still contains original point
		// swap x and y then invert y
	    int t=p.x;
			p.x=p.y; p.y = t;
			p.x = (map(p.x, TS_MINX, TS_MAXX, 0, screenWidth));
			p.y = (map(p.y, TS_MINY, TS_MAXY,  screenHeight,0));
		
		}	  
		else{
			// scale from 0->1023 to tft.width
			p.x = (map(p.x, TS_MINX, TS_MAXX, 0, screenWidth));
			p.y = (map(p.y, TS_MINY, TS_MAXY, 0, screenHeight));
		}
   p.x+=xoffset;
   p.y+=yoffset;
   
    touched=p; // save the position
		if(DEBUGTOUCH){
		Serial.print(" Width:"); Serial.print(screenWidth);
    Serial.println(" Height"); Serial.print(screenHeight);
    
		Serial.print(" Final("); Serial.print(p.x);
		Serial.print(", "); Serial.print(p.y);
		Serial.println(")");
		
		Serial.print(" Max("); Serial.print(TS_MAXX);
    Serial.print(", "); Serial.print(TS_MAXY);
    Serial.print(")");
    
		Serial.print(" Min("); Serial.print(TS_MINX);
    Serial.print(", "); Serial.print(TS_MINY);
    Serial.println(")");
		Serial.print(" Button("); Serial.print(buttonSize.x);
    Serial.print(", "); Serial.print(buttonSize.y);
    Serial.println(")");
    
		Serial.print(" Menu:"); Serial.println(menuState);
    
		}
		   // check button events
		int grid = p.x/buttonSize.x;

		grid+=(p.y/buttonSize.y*10);
		buttonEvent(grid);
		   

		 
		
		if (((p.y-PENRADIUS) > 0) && ((p.y+PENRADIUS) < tft.height())) {
		  tft.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
		  
		}
	}

  
}

 boolean isbst(time_t t){ 
 // checks if date is in bst
        if (month(t) < 3 || month(t) > 10)  return false; 
        if (month(t) > 3 && month(t) < 10)  return true; 
        int previousSunday = day(t) - weekday(t);
        if (month(t) == 3) return previousSunday >= 25;
        if (month(t) == 10) return previousSunday < 25;
        return false; // this line is never gonna happen
    }

tmElements_t bstadjust(time_t checkElements){
// if(isbst(checkElements.tm_day,checkElements.tm_mon,weekday())){
    
  
} 

void gettime(){ 
//      Loads all of the time variables from GPS>RTC>System clock   //

  millisNow=millis(); // timer for local loop triggers
  timeNow=now(); // get the current time stamp
  tm = timeNow;
      
  if(isbst(timeNow)){
      tm.Hour=tm.Hour+1;
      if(DEBUGTIME)Serial.println("Date is in BST");
    
    }else{
      if(DEBUGTIME)Serial.println("Date is not in BST");
    
    };
}

void clockSetup() {  // Clock Setup Routine
// wdt_disable(); // disable the watchdog
    
  delay(2000);// let rtc settle
    Serial.println("Stuart's LED RTC - GPS and DS1307RTC V0.1");
    Serial.println("-----------------------------------------");
  
  // Set the processor time
  setTime(0,0,0,16,10,1964);
  
  unsigned long t=millis();
  waitcheckTime = t + intervalcheckTime;  
  waitcheckButtons = t + intervalcheckButtons;
  
  NextTimeSyncTime=t; // trigger initial time sync
  
  
}

void print2digits(int number) {
  if (number > 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}




void checktimesync(){
  if(millisNow>NextTimeSyncTime){
    NextTimeSyncTime=millisNow+NextTimeSyncInterval;
    if(DEBUGTIME)Serial.println("syncing time <*****************************************");
    // Time precedence
    // GPS if Valid
    // RTC if valid
    // system clock

    // set system time to RTC
    // need to validate data before setting
    
  }
}
  

void checkReport() {
    if( millisNow> reportTime){ // if we're due
      reportTime=millisNow+reportInterval; // reset interval
      Serial.println("*******************************");
      
      Serial.print("Onboard Clock:"+String(hour(timeNow))+":"+ String(minute(timeNow))) +":"+ String(second(timeNow));  // Print system time
      if(isAM(timeNow)) Serial.println("AM");
      if(isPM(timeNow)) Serial.println("PM");
      // now() time in seconds since 1/1/70
      Serial.println(String(day(timeNow))+":"+String(month(timeNow))+":"+String(year(timeNow)));             // The day now (1-31)
      Serial.println("DOTW:"+String(weekday(timeNow)));         // Day of the week, Sunday is day 1
      //Serial.println(String(dayStr(weekday(time_now)))+" "+String(day(time_now))+String(monthStr(month(time_now))));
      //Serial.println(String( monthShortStr(month(time_now)))+" "+String(dayShortStr(weekday(time_now))));
      Serial.println("Time Status:"+String(timeStatus()));
      Serial.println("Time Now:"+String(timeNow));

      Serial.print("tm. Time = ");
      print2digits(tm.Hour);
        Serial.write(':');
        print2digits(tm.Minute);
        Serial.write(':');
        print2digits(tm.Second);
        Serial.print(", Date(D/M/Y) ");
        Serial.print(tm.Day);
        Serial.write('/');
        Serial.print(tm.Month);
        Serial.write('/');
        Serial.print(tmYearToCalendar(tm.Year));
        Serial.println();
      
      
    }
    
    
  }




String timestring()   {
	String s= 
	twochars(tm.Hour)+":"+twochars(tm.Minute)+":"+twochars(tm.Second);
return s;
}

String longDateString(time_t t){
  String thisstring;
    thisstring=dayShortStr(weekday(t));
      thisstring+=" ";
      thisstring+=String(day(t));
      thisstring+=" ";
      thisstring+=String(monthShortStr(month(t)));
      thisstring+=" ";
      thisstring+=String(year(t));
  return thisstring;
}
String datestring(time_t t){
  String thisstring;
    thisstring=
	thisstring+=String(year(t));
      thisstring+=":";
      thisstring+=twochars(month(t));
      thisstring+=":";
      thisstring+=twochars(day(t));
      
  return thisstring;
}
String twochars(int number){
  if(number==0) {
    return"00";
  }
  else {
    if (number > 0 && number < 10)  {
      return "0"+String(number);
    }
    else  {
      return String(number);
    }
  }
}
void decDays(){
		if (tm.tm_day != 0)
							{  
								tm.Day-- ;
							}
							else
							{ // oh poo
			//				  decMonth();
							  tm.Day=daysInMonth[tm.Month];
	}
	setClock();
}

void decHours(){
		if (tm.Hour != 0)
					{  
			tm.Hour-- ;
		}
		else
		{ // midnight
			tm.Hour=23;
		}
	setClock();					
}

void decMonth(){
	if (tm.Month != 0)	{  
			tm.Month-- ;
							}
							else
							{
		//					  tm.Year--;
							  tm.Month=12;
	}
	setClock();
}

void setClock(){
	setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year); 
}

void clearScreen(){
	tft.fillRect(0, (buttonSize.y)+2, screenWidth, screenHeight-buttonSize.y, BLACK);
	 
}

void buttonEvent(int inp){
  //dm.setLED( 1,inp);
	if(DEBUGBUTTONS){
	  Serial.print("Button:");
	  Serial.println(inp);
	}
	if(inp==0)menuState=0;  // NOW button resets state
	switch (menuState) {
		case 0: {// Root Display 
			switch (inp) {
				case 0: // Now
				{
					menuState=0;
					clearScreen();
				}
				break;
		
				case 1: // Set
				{
					menuState=1;
					clearScreen();
					drawEditButtons();
				
				}
				break;
				case 2: // Delete
				{
					menuState=2;
				}
				break;
				case 3: // Edit
				{
					menuState=3;
				}
				break;
				case 4: // New
				{
					menuState=4;
				}
				break;
				case 5: // List
				{
					menuState=5;
				}
				break;
				case 6: //list
				{
					
				}
				break;
				
			}
		}
		break;
		case 1:{ // clock set
			switch (inp) {
				case 50: { // dec year
					tm.tm_year-- ;
					setClock();
					Serial.println("Dec year!")	;
				}
				break;
				
				case 51:  { // decrement months
					decMonth();
				
				}
				break;
				
				case 52: { // dec days
					decDays();
				}
				break;
				case 53:  	  { // decrement Hours
					if (tm.Hour != 0)
					{  
						tm.Hour-- ;
					}
					else
					{
					  tm.Hour=23;
					}
					setClock();		
				  
				}
				break;
				case 54:  	  { // decrement Minutes
					if (tm.tm_min != 0)
					{  
						tm.tm_min-- ;
					}
						else
					{
						  tm.tm_min=59;
					}
						// now write it to RTC
					setClock();
				}
				break;
							 
			}
		}
		break;
	}
  
}

void drawEditButtons(){
  // show edit triangles
    for(int i=0; i<6; i++) {
      tft.drawTriangle(
        (i*buttonSize.x)+(buttonSize.x/2)    , 6*buttonSize.y , // peak
        i*buttonSize.x, 5*buttonSize.y+1, // bottom left
        (i+1)*buttonSize.x, 5*buttonSize.y+1, // bottom right
        // tft.color565(0, 0, i*24)
        RED
    );
	 tft.drawTriangle(
        (i*buttonSize.x)+(buttonSize.x/2)    , 3*buttonSize.y , // peak
        i*buttonSize.x, 4*buttonSize.y, // bottom left
        (i+1)*buttonSize.x, 4*buttonSize.y, // bottom right
        // tft.color565(0, 0, i*24)
        RED
    );
    Serial.println("edit buttons drawn");
    }
}

void showClock() {

  
  // for serial debugging
  if(DEBUGTIME){
   Serial.print(timestring(tm));
   }
	if(tm.Second!=lastsecond){
   if(menuState==1){  // clock set mode
	  tft.fillRect(0, (4*buttonSize.y)+2, tft.width(),buttonSize.y-1, BLACK);
	   
		tft.setCursor(0, (4*buttonSize.y)+4);
		tft.setTextColor(GREEN);
	  tft.setTextSize(2); 
	//tft.print(tm.tm_year);
	//  tft.print(":");
	//  tft.setTextSize(3);	  
	//  tft.print(twochars(tm.tm_mon));
	//  tft.print(":");
	//  tft.print(twochars(tm.tm_day));
	//  tft.print(":");
	//  tft.print(twochars(tm.tm_hour));
	//  tft.print(":");
	//  tft.print(twochars(tm.tm_min));
	//  tft.print(":");
	//  tft.print(twochars(tm.tm_sec));
   
		tft.print(datestring(tm));
		tft.setTextSize(3);	  
	  	
		tft.print(timestring(tm));
   } else {
	   tft.fillRect(0, (3*buttonSize.y)+2, tft.width(),buttonSize.y-1, BLACK);
	   
		tft.setCursor(0, (3*buttonSize.y)+4);
		tft.setTextColor(GREEN);
	  tft.setTextSize(2);
	  /*tft.print(tm.tm_year);
	  tft.print(":");
	  tft.setTextSize(3);	  
	  tft.print(twochars(tm.tm_mon));
	  tft.print(":");
	  tft.print(twochars(tm.tm_day));
	  tft.print(":");
	  tft.print(twochars(tm.tm_hour));
	  tft.print(":");
	  tft.print(twochars(tm.tm_min));
	  tft.print(":");
	  tft.print(twochars(tm.tm_sec));
	  */
	  tft.setTextSize(3);	  
	  
	  tft.print(datestring(tm));
	  tft.print(timestring(tm));
   }
	  
	  
	  
	  lastsecond=tm.Second;
	}
}

