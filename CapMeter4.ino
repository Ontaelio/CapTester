/*
 Capacitance Meter

 Original code: tmd3 on Arduino forum
 				http://forum.arduino.cc/index.php?topic=174338.0 post#21
 Slight changes and improvements: Dmitry Reznikov, 2015
 
 Defaults: capacitor pin 7, voltage reference pin 6;
 these can be switched - see macros definitions below.
 Capacitor to be measured between capacitor pin and GND.
 Resistor, value = Rc, beween capacitor pin and cPin.
 Resistor Rd, value < Rc/5 & > 200, between dPin and capacitor pin.

 cPin ----> Rc ----> capacitor pin ----> capacitor_being_tested ----> GND

 +5v ----> R2 ----> voltage reference pin ----> R1 ----> GND

 capacitor pin ----> Rd ----> dPin
 
*/

// *** Define setup section - change according to your layout

#define smallRc 10000.0  // default Rc resistance to measure 1nF and up
#define bigRc 1000000.0  // bigger Rc resistance to measure pF capacitors

#define Rd 510.0  // drain resistor
#define R1 984.0  // to GND
#define R2 1994.0 // to 5v
#define clockRate_us 16.0

#define dPin 8    // drain, any pin will do, 8 by default 
#define cPinNF 9  // test voltage pin for small resistor (nF meter), default 9
#define cPinPF 10 // test voltage pin for big resistor (pF meter), default 10
				  // you can change these to pins 3 and 4, see macros below
				  // if other pins are used, add/change port manipulation macros below

#define setupNonFpin 12 // connect this pin to GND to disable nF mode
#define setupNopFpin A0 // connect this pin to GND to disable pF mode	

#define pFmodeWait 10 // cycles to wait before changing to pF mode (default 10)

// *** the following macros start timer with either falling or rising edge.
// *** falling: reference voltage on pin 6, measured capacitor on pin 7
// *** rising: reference voltage on pin 7, measured capacitor on pin 6
#define START_TIMER TCCR1B = (1<<CS10)  // falling
//#define START_TIMER TCCR1B = (1<<CS10) | (1<<ICES1)  // rising

// *** macros to provide voltage to pins 4/3 or 9/10, depending on layout 
//#define VPIN_HIGH(X) PORTD |= (1 << X)  // pin 4/3
#define VPIN_HIGH(X) PORTB |= (1 << X)  // pin 9/10
// *** X for VPIN_HIGH macro
#define smallCap 1 // 1 for pin 9 (PORTB); 4 for pin 4 (PORTD)
#define bigCap 2   // 2 for pin 10 (PORTB); 3 for pin 3 (PORTD)


// *** macros for visualization depending on your serial monitor abilities. Defaults work with RealTerm
#define PRINT_NEW_LINE Serial.print("\r") // carriage return; display resultis in one line
//#define PRINT_NEW_LINE Serial.println()  // use this if your serial monitor doesn't support CR (Arduino IDE)
#define PRINT_TAB Serial.print("\t")    // tab
//#define PRINT_TAB Serial.print("--- ")  // use this if your serial monitor doesn't show tabs (UECIDE)

// *** the end of define setup section

volatile uint8_t TIFR1_Copy;
volatile uint16_t ICR_Copy;
volatile uint8_t ICR_Flag = 0;
volatile uint16_t TOV1_Ctr;

bool pFmode = false;
bool NonFmode = false;
bool NopFmode = false;
bool modeChange = true;
byte adjustmentCounter = 22;
byte pFmodeCounter = 0;
float nFadjustment = 0;
float pFadjustment = 0;
float jitMin = 0;
float jitMax = 0;
byte pinSelect = smallCap;

float C; 
float k;
uint16_t dischargeTime;

void calibration()
 {if (adjustmentCounter == 22) // first measure = nF adjustment through small resistor
 	{
 		nFadjustment = C;
 		Serial.print("nF adjustment: -");
 		Serial.print((nFadjustment*1000000),3); 
 		Serial.println(" pF");
 		adjustmentCounter--;
 		k = 1/(clockRate_us*bigRc*log((R1+R2)/R2));
 		pFmode=true;
 		pinSelect = bigCap;
 		delay (1000);
 		return;
 	} else
  if (adjustmentCounter == 21)  // second measure = pF adjustment through big resistor
 	{
 		pFadjustment = C;
 		Serial.print("pF adjustment: -");
 		Serial.print((pFadjustment*1000000),3); 
 		Serial.println(" pF");
 		adjustmentCounter--;
 		delay (1000);
 		return;
 	} else
  {
 	C = C - pFadjustment;
 	if (C < jitMin) {jitMin = C;}
 	if (C > jitMax) {jitMax = C;}
 	Serial.print("Testing...  ");  // show the jitter on big capacitor for a sec; maybe restart is needed
 	PRINT_TAB;	
 	Serial.print(C*1000,3); Serial.print(" nF    ");
 	PRINT_TAB;
 	Serial.print((C*1000000),2); Serial.print(" pF                      "); 
 	PRINT_NEW_LINE;
 	adjustmentCounter--;
 	if (adjustmentCounter == 0)
 	  {
 	  	Serial.print("Jitter: ");
 	  	Serial.print((jitMin*1000000),2);
 	  	Serial.print(" to ");
 	  	Serial.print((jitMax*1000000),2);
 	  	Serial.println(" pF                                  ");
 	  	Serial.println("Ready");
 	  }
  };
 }

void displayResults()
{
if (pFmode)  // small capacitor meter
 {
 C = C - pFadjustment;  // substract 'stray capacitance'
 Serial.print("  ****   ");
 PRINT_TAB;	
 Serial.print(C*1000,3); Serial.print(" nF     ");
 PRINT_TAB;
 Serial.print((C*1000000),2); Serial.print(" pF                         "); 
 PRINT_NEW_LINE;

 if (((C < 0.0000005) | (C > 0.002)) && (!NonFmode)) // switch to nF mode if needed
 	{
 	 pFmode = false;
 	 pFmodeCounter=0;
 	 k = 1/(clockRate_us*smallRc*log((R1+R2)/R2));
 	 pinSelect = smallCap;
 	};
 
 } else  // big capacitor meter
 {
 C = C - nFadjustment;  // substract 'stray capacitance'
 Serial.print(C,2); Serial.print(" uF   ");
 PRINT_TAB;
 Serial.print((C*1000),3); Serial.print(" nF     ");
 PRINT_TAB;
 Serial.print("  ****                            ");
 PRINT_NEW_LINE;

 if (((C < 0.002) && (C > 0.0000008)) && (!NopFmode)) // switch to pF mode if needed
 	{
 		pFmodeCounter++;
 		if (pFmodeCounter > pFmodeWait) // some delay before switch to avoid false triggers
 		{pFmode = true;
 		 k = 1/(clockRate_us*bigRc*log((R1+R2)/R2));
 	     pinSelect = bigCap;
 	     pFmodeCounter = 0;
 	    };
 	}
 };
} 

void clearPrint()
{
	PRINT_NEW_LINE;
 	delay(1000);
 	Serial.print("                                                     ");
 	PRINT_NEW_LINE;
}

void checkSettings()
{
	MCUCR &= ~(1 << PUD); // enable pullup registers
	pinMode(setupNonFpin, INPUT_PULLUP);
    pinMode(setupNopFpin, INPUT_PULLUP);
	if ((digitalRead(setupNonFpin)==LOW) && (!NonFmode))
	{
		Serial.print("Disabling nF mode...                                        ");
		NonFmode = true;
		pFmode = true;
 		k = 1/(clockRate_us*bigRc*log((R1+R2)/R2));
 	    pinSelect = bigCap;
 	    pFmodeCounter = 0;
 	    clearPrint();
 	}
	else
	if ((digitalRead(setupNonFpin)==HIGH) && (NonFmode))
	{
		Serial.print("Enabling nF mode...                                         ");
		NonFmode = false;
		clearPrint();
	}
	else
	if ((digitalRead(setupNopFpin)==LOW) && (!NopFmode))
	{
		Serial.print("Disabling pF mode...                                        ");
		NopFmode = true;
		pFmode = false;
 		k = 1/(clockRate_us*smallRc*log((R1+R2)/R2));
 	    pinSelect = smallCap;
 	    pFmodeCounter = 0;
 	    clearPrint();	
	}
	else
	if ((digitalRead(setupNopFpin)==HIGH) && (NopFmode))
	{
		Serial.print("Enabling pF mode...                                         ");
		NopFmode = false;
		clearPrint();
	}
	MCUCR |= (1 << PUD); // disable pullup registers
}
	
 	

void setup() {
 Serial.begin(115200);
 Serial.println("Starting up");


// check the mode control pins and display the programmed resistor values  
 pinMode(setupNonFpin, INPUT_PULLUP);
 pinMode(setupNopFpin, INPUT_PULLUP);

 if (digitalRead(setupNonFpin)==LOW) {NonFmode = true;}
 if (digitalRead(setupNopFpin)==LOW) {NopFmode = true;}

 if (NonFmode && NopFmode)
 	{Serial.println("Mode change disabled");
 	 modeChange = false;
 	 NonFmode = false; NopFmode = false;}
 Serial.print("nF resistor: ");
 Serial.print(smallRc);
 if (NonFmode) {Serial.print(" (disabled)");}
 Serial.println();
 Serial.print("pF resistor: ");
 Serial.print(bigRc);
 if (NopFmode) {Serial.print(" (disabled)");}
 Serial.println();

// k is the multiplier used to calculate capacitance based on resistor values
// it changes later in the code with mode changes
 k = 1/(clockRate_us*smallRc*log((R1+R2)/R2));
 
 
 MCUCR |= (1 << PUD);  // Globally disable pullup resistors
 TCCR1B = 0;  // Noise Canceler off; Normal mode; falling edge; stopped
 TCCR1A = 0;  // Normal mode; 
 TIMSK1 = (1 << ICIE1) | (1 << TOIE1); // Input Capture and Overflow interrupts

 DIDR1 |= (1<<AIN1D)|(1<<AIN0D); // Disable digital buffer on comp inputs
 ACSR = (1 <<  ACIC); // Enable comparator capture

 pinMode(dPin,OUTPUT); 
 pinMode(cPinNF,OUTPUT);
 pinMode(cPinPF,OUTPUT);
 digitalWrite(dPin,LOW);
 digitalWrite(cPinNF,LOW); 
 digitalWrite(cPinPF,LOW); // All pins low - discharge

 delay(1000); // Wait plenty of time for discharge
}

void loop() {

 
 ICR_Flag = 0; // Initilize counters and flag
 TOV1_Ctr = 0;
 TCNT1 = 0;

 pinMode(dPin,INPUT); // dPin set to input - high impedance
 if (pFmode) {pinMode(cPinNF,INPUT);pinMode(cPinPF,OUTPUT);} else {pinMode(cPinPF,INPUT); pinMode(cPinNF,OUTPUT);}

 cli(); // No interrupts between starting charge and starting timer
 VPIN_HIGH(pinSelect);  // start charging, see defines above
 START_TIMER; // start Timer1, see defines above
 sei(); // Let interrutps happen now

 while (!ICR_Flag) {} // Wait for charging to finish

 pinMode(dPin,OUTPUT);
 pinMode(cPinNF,OUTPUT);
 pinMode(cPinPF,OUTPUT);
 digitalWrite(dPin,LOW);
 digitalWrite(cPinNF,LOW); 
 digitalWrite(cPinPF,LOW); // dPin and test voltage pins low - start discharge

 // Calculate capacitance
 // Adjust overflow counter - if TOV1 is on, and ICR is low -
 if ((!(ICR_Copy & (1 << 15))) && (TIFR1_Copy & (1 << TOV1))) {
   TOV1_Ctr++;
 }
 C = k * (float)(((uint32_t)TOV1_Ctr << 16) | ICR_Copy);

 if (adjustmentCounter > 0) // calculating 'stray capacitances' on the first run

 {calibration();}
 
 else

 {displayResults();}

	
// start discharging

 dischargeTime = 1 + (uint16_t)(10.0*Rd*C/1000.0);  
 delay(dischargeTime); // Wait at least 10 time constants
 if (dischargeTime < 250) {
   delay(250-dischargeTime); // Wait some more to limit printing
 }

if ((adjustmentCounter == 0) && modeChange) checkSettings();
}

ISR(TIMER1_CAPT_vect) {
 TCCR1B = 0;          // Stop Timer1 - CS12:CS10 = 0
 TIFR1_Copy = TIFR1;  // Get overflow interrupt status
 TIFR1 = (1 << TOV1); // Clear overflow
 ICR_Copy = ICR1;     // Get the ICR
 ICR_Flag = 1;        // Set the flag
}

ISR(TIMER1_OVF_vect) {
 TOV1_Ctr++;  // Bump the overflow counter
}
