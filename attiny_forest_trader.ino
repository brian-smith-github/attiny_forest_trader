#include <EEPROM.h>
#include "attinyarcade.h"
#include "font.h"
#include "trees.h"
#include "bikes.h"

#define EEPROM_START 100 // save-state location in EEPROM (~25 bytes)
#define DELAY 7 // frame-rate slow-down in ms. 8 for atttiny, 10 for Linux


typedef struct 
{
  byte marker;
  unsigned long int cash;
  byte day,sun;
  byte cargo[11],curcargo;
  byte x0,y0,r0;
  byte job;
} game;

game g; // player stats in game
byte x1,y1,r1=0,d; // destination position and distance to it
byte count,posn,line[128],col[8];
byte *b; // pointer to char buffer
const byte *p; // pointers to PROGMEM text data
unsigned short dd,dy; // 16-bit euclidean distance calculations
byte text[21],tpos; // 21 characters per line, plus a pointer
unsigned short tmap[43]; // 43x16 trees map (86 bytes)
byte i,a,n,s,w,m;
byte x,y,dest=0;
byte density=3; // forest density, 1 to 9 is good
byte of;  // initial tree offset
byte bdir=0; // bike sprite direction
byte dir=3;  // travel direction for house rendering
byte frame=0;
byte startstop=0; // for acceleration/deceleration bits

#define DIALSIZE 36 // 6x8 damage dial on left + 30x8 distance on right
byte dials[DIALSIZE];


//-----------------------------------------------------------
byte buttons_db()
{
  byte b;
  unsigned short count=0;
  
  b=buttons();
  while(b==0) {b=buttons(); _delay_ms(1); 
              count++; if (count==30000) powerDown(0);}
  _delay_ms(50); // debounce
  while(buttons()!=0);
  _delay_ms(100); // debounce
  return(b);
}

//---------------------------------------------------------
// load state from EEPROM
void load_state()
{
  EEPROM.get(EEPROM_START,g);

}
  

//----------------------------------------------------------
// save state to EEPROM
void save_state()
{
  EEPROM.put(EEPROM_START,g);
}

//------------------------------------------------------------
// Initialize the saved state to standard
#define FUEL 8
#define HEALTH 9
#define MAXCARGO 10
void initial_state()
{
  g.marker=25; 
  g.day=1; g.sun=1;
  g.cash=100; g.cargo[FUEL]=200;
  for (i=0; i<8; i++) g.cargo[i]=0;
  g.curcargo=0; g.cargo[MAXCARGO]=3;  
  g.cargo[HEALTH]=3;
  g.x0=96; g.y0=20; g.r0=76; // Lexi
  g.job=72;
  if (EEPROM.read(EEPROM_START)!=g.marker)  save_state();
}

//---------------------------------------------------------
// write a text string to the text buffer....
void add_text()
{
  byte a;
  
  textloop:
  a=pgm_read_byte(p++);
  if (a!=0) {
    a-=49;
    text[tpos++]=a;
    goto textloop;   
  } 
}

//-----------------------------------------------------------
// write a 16-bit number, backwards, into the text buffer
void add_number(unsigned short num)
{ 
  byte a;

  numloop:
  a=num%10; text[tpos--]=a; 
  num/=10; if (num!=0) goto numloop; 
}


//-----------------------------------------------------------
// write a 32-bit cash value, backwards, into the text buffer
void add_cash()
{
  unsigned long int num=g.cash;
  numloop:
  a=num%10; text[tpos--]=a; 
  num/=10; if (num!=0) goto numloop; 
}

//--------------------------------------------------------------
// write an 8-bit x.y mileage backwards into the text buffer
void add_mileage(byte d)
{
  byte s;
  
  text[tpos--]='m'-49;
  s=d%10; text[tpos--]=s; 
  text[tpos--]='.'-32; d/=10;
  milage_loop:
  s=d%10; text[tpos--]=s; 
  d/=10; if (d!=0) goto milage_loop;
}

//----------------------------------------------------------
// send the line[] buffer out to the display.
void write_line()
{
   byte s;
   byte *b;
   
   b=line;
   s=128; while(s!=0) {i2c_sendByte(*b++); s--;}
}

//--------------------------------------------------------------
// generate the line[] buffer from the text[] buffer
// and (usually) write that straight out to the screen
void write_text()
{
  byte *b,s;
  int dd;
  
  b=line; *b++=0;
  for (s=0; s<21; s++) {
    dd=text[s]; dd*=5; text[s]=46;  // replace with underline/space
    for (m=0; m<5; m++)  {a=pgm_read_byte(&font[dd++]); *b++=a;}
    *b++=0;
  }
  *b++=0;
  if (tpos!=128) write_line();
  tpos=0; // reset for next line
}

//--------------------------------------------------------
unsigned char r = 0;
void xorshift()  // 8-bit PRNG. Period 255 (skips '11')
{
  // shift byte left, and if carry is not set, XOR with 0x1d.
  if (r&128) r<<=1; else {r<<=1; r^=0x1d;} // simulate carry bit in C
}



//--------------------------------------------------------
PROGMEM const byte cons[]="bbbccxddfghklvmmnnnprrssstttgwny";
PROGMEM const byte vowel[]="aeoiu";
void name_town()
{
  byte len;
  len=r%4+4; a=len;

  while(len!=0) {
     text[tpos]=pgm_read_byte(&cons[r%32])-(32+17);
     if (len==a) text[tpos]-=32; // Capitalize the name
     tpos++; len--;
     if (len) {text[tpos++]=pgm_read_byte(&vowel[r%5])-(32+17);  len--; }
     xorshift();	 
  }
}

//---------------------------------------------------------------------------------
// generate the z buffer for this frame....
void gen_zbuf()
{
  int i,n,a;
  byte of,s,w;
   
  for (i=0; i<128; i++) line[i]=0;
  a=1;
  if (d>2) for (n=15; n>=0; n--) {
    s=pgm_read_byte(&tstart[n]); // tree sprite start
    w=pgm_read_byte(&twidth[n]); // tree sprite width
    m=pgm_read_byte(&mstart[n]);  // map start at this depth
    of=pgm_read_byte(&offset[n]); // sprite offset
    for (i=0; i<128; i++) {
       if (line[i]==0 && (tmap[m]&a))  line[i]=s+of;
       of++; if (of==w) {of=0; m++;}
    }
    a*=2;
  } else if (d>1) {  // inital approach.
    a=128-dir*2; if (a<64) a+=64; else a-=64; a-=frame/4;
    for (i=frame/2+1; i!=0; i--) {
       line[a]=pgm_read_byte(&house[0]);
       a++; if (a>127) a=0;
      
    }
  } else { // final approach, add house render instead
    a=124-dir*2; if (a<64) a+=64; else a-=64; a=a-frame;
    for (i=0; i<frame*5+15; i++) {
       line[a]=pgm_read_byte(&house[frame+1]);
       a++; if (a>127) a=127;
       
    }
    startstop+=3; // decelerate
  }
}

//----------------------------------------------------------------------
// generate the on-screen dials 
void gen_dials()
{
  byte i; 
  
  text[0]=g.cargo[HEALTH];
  tpos=5; add_mileage(d);
  tpos=128; write_text(); // generate line[] but dont send to screen
  for (i=0; i<36; i++) dials[i]=line[i]; 
}

//-----------------------------------------------------------------------
// draw sky, trees and bike

#define STARS 10
byte star_x[STARS]={11,25,31,43,52,56,66,86,107,120};
byte star_y[STARS]={0,3,2,0,1,3,1,2,0,2};
byte star_low=0;

void render()
{
  byte b,x,star;
  int a,i,n; // bjs debug, seems to help, pain in the arse.
  
  display_dataMode();
  star_low=0; for (i=1; i<STARS; i++) if (star_x[i]<star_x[star_low]) star_low=i; 
  star=star_low;
  for (i=0; i<128; i++) {
    a=line[i]*8; // tree slice for this screen column
    for (n=0; n<8; n++) {
        b=pgm_read_byte(&trees[a]);
	if (g.sun) b=~b; // daytime
        col[n]=b;
        a++;
     }
     // add a star?
     if (!g.sun && i==star_x[star]) {
       col[star_y[star]]|=4;
       star++; if (star==STARS) star=0;
     }
     if (i<6 || i>97) { // add dials at bottom of these columns...
       a=i; if (a>97) a=a-98+6;
       b=dials[a]; col[7]=b;
     } else if (i>47 && i<80) { // add bike at bottom of these columns.
       n=bdir+i*2-96; 
       b=col[6];
       x=pgm_read_byte(&bmask[n]); x=~x;
       b&=x;
       x=pgm_read_byte(&bikes[n]);
       b|=x;
       n++;
       col[6]=b;
       
       b=col[7];
       x=pgm_read_byte(&bmask[n]); x=~x;
       b&=x;
       x=pgm_read_byte(&bikes[n]);
       b|=x; 
       col[7]=b;
     }
     // lastly, write out the 8 bytes of this column...
     for (n=0; n<8; n++) i2c_sendByte(col[n]);
  }
  i2c_stop();
}

//---------------------------------------------------------------------------
PROGMEM const byte gameover[]="Game_Over";
void crash()
{
  int i,n;
 
  g.cargo[HEALTH]--;
  if (g.cargo[HEALTH]==0) d=0; // force end
  for (i=0; i<10; i++) {
    for (n=0; n<127; n++) line[n]=line[n+1];
    render(); _delay_ms(5);
    for (n=127; n>0; n--) line[n]=line[n-1];
    render(); _delay_ms(5);
  } 
  _delay_ms(1000); 
  tmap[21]=0; // allow safe resume
  startstop=10; // initial acceleration.
}

//--------------------------------------------------------------------------
PROGMEM const byte welcome[]="Welcome_To_";
PROGMEM const byte m_items[]="Food\0Medicine\0Liquor\0Textiles\0\
Spices\0Tech\0Gems\0Luxuries\0Refuel\0Repairs\0Cargo";

int journey() {
   int i;
   byte x=0;

  //d=4; density=1; // testing....
  if (g.r0==g.job) g.job++; // on to next job if completed.
   display_init(1);
  for (i=0; i<43; i++) tmap[i]=0;
  for (i=0; i<DIALSIZE; i++) dials[i]=255;
  startstop=10; // initial acceleration.
  
  g.cargo[FUEL]-=d; 
  while(d>0) {
    for (i=0; i<43; i++) { // randomly place trees....
      x+=r; xorshift();
      if (x<density && d>4) tmap[i]|=32768;
    }
    gen_dials();
    gen_zbuf();
    render();
   
    if (tmap[21]&1) crash();
    for (n=0; n<43; n++) tmap[n]>>=1; // move bike forward
    for (i=0; i<6+startstop; i++) _delay_ms(DELAY);
    if (startstop>0) startstop--;

    if (buttons()==RIGHT_BUTTON) { 
      for (i=0; i<42; i++) tmap[i]=tmap[i+1];tmap[42]=0; 
      bdir=128; // bike sprite
      for (i=0; i<STARS; i++) star_x[i]=(star_x[i]-4)&127;
    } else  if (buttons()==LEFT_BUTTON) {
      for (i=42; i>0; i--) tmap[i]=tmap[i-1]; tmap[0]=0;
      bdir=64;
      for (i=0; i<STARS; i++) star_x[i]=(star_x[i]+4)&127;
    } else bdir=0;
    
    frame++; if (frame==16) {frame=0; if (d>0) d--;}   
  }
  render();
  display_init(0);
  g.x0=x1; g.y0=y1; g.r0=r1; r=r1; // jump to new site.
  if (g.sun) g.sun=0; else {g.sun=1; g.day++;}
  if (g.cargo[HEALTH]!=0) {tpos=2; p=welcome; add_text(); 
                          name_town(); save_state();}
                    else  {p=gameover; add_text(); load_state();}
   write_text();
  _delay_ms(3000); 
  return(0);
}

//---------------------------------------------------------
// Prices - list/buy/sell/maintain bike
// (trading prices are based purely on x and y.)

PROGMEM const byte BUYHEADER[]="BUY\\EQUIP";
PROGMEM const byte SELLHEADER[]="SELL";
PROGMEM const byte CASH[]="Cash`";
PROGMEM const byte HAVE[]="Have`";
PROGMEM const byte UNITS[]="units`";
PROGMEM const byte CARGO[]="Cargo`";

#define LIST 0
#define BUY 1
#define SELL 2
#define DOLLAR 10
int prices(int opt)
{
  unsigned short scale,price; // ensure money=2 bytes
  byte space; // room for more goods?
  int i;
  const byte *p0,*p1;
 
  p0=m_items;
  i=0;
  w=8; if (opt==BUY) w=11; // 3 extra items, fuel/repair/cargo
  while(i!=w) {
    if (opt!=LIST) {
      tpos=6; p=BUYHEADER; 
      if (opt==SELL) p=SELLHEADER;
      add_text(); write_text(); write_text();
    }
    p=p0; add_text(); p1=p; // add goods name
    price=8<<i; // base price scale range   
    if (i&1) scale=x; else scale=255-x; price+=256*scale/(1<<(12-i));
    if (i&2) scale=y; else scale=255-y; price+=256*scale/(1<<(12-i));
    if (g.r0==g.job && i==(g.job&7)) price<<=2;  // pay-off for job completion
    if (i&8) price=i<<g.cargo[MAXCARGO];  //upgrades increasingly expensive
    if (i==8) price=2; // however, refuel should be super-cheap 
   
    tpos=20; add_number(price); text[tpos]=DOLLAR; write_text();
    if (opt!=LIST) { // buy/sell
      write_text();
      p=HAVE; add_text(); tpos=10; add_number(g.cargo[i]); write_text();              
      p=CASH; add_text(); tpos=12; add_cash();
      text[tpos]=DOLLAR; write_text(); 
            
      write_text();  // padding
      p=CARGO; add_text(); tpos=20; add_number(g.cargo[MAXCARGO]);
      tpos=17; text[tpos]='/'-32;
      tpos=15; add_number(g.curcargo); 
      write_text();
      
      while(buttons()==0);
      a=buttons_db();
      if (a==RIGHT_BUTTON) { // buy or sell...
        space=0;
        if (i<8 && g.curcargo<g.cargo[MAXCARGO]) space=1;
        if (i==FUEL && g.cargo[FUEL]<200) space=1;
	if (i==MAXCARGO && g.cargo[MAXCARGO]<9) space=1;
       	if (i==HEALTH && g.cargo[HEALTH]<9) space=1;
        if (opt==BUY && g.cash>price && space) {
	        g.cargo[i]++; g.curcargo++; g.cash-=price;
	        if (i&8) g.curcargo--; // maintenance != cargo
	        if (i==FUEL) g.cargo[FUEL]+=9; // 10 units per purchase
	      }
	      if (opt==SELL && g.cargo[i]!=0) { g.cash+=price; g.curcargo--; g.cargo[i]--;}
      }  else {p0=p1; i++;}     
    } else {p0=p1; i++;}
  }
}

//------------------------------------------------------------
// generate list of towns in +/-31 range of (x0,y0)
void within_range()
{
  count=0;

  x=0; y=0; r=0;
  while(r!=128) {
    y+=r; xorshift(); x+=r;
    
    if (g.x0>x) dd=g.x0-x; else dd=x-g.x0;
    if (g.y0>y) dy=g.y0-y; else dy=y-g.y0;
    if (dd>=32 || dy>=32) {dd=0; dy=0;} // ignore out-of-range
    // get squared Euclidean distance...
    dd*=dd; dy*=dy; dd+=dy; dd<<=3;
    // 7-stage integer sqrt
    d=0; i=64; while(i!=0) {d+=i; dy=d*d; if (dd<dy) d-=i; i>>=1;}
    // ignore towns we don't have the fuel to get to...
    if (d>g.cargo[FUEL]) d=0;
    // insertion sort based on d..... (but only if d!=0)
    if (d) {
      posn=count; while(posn>0 && line[posn-1]>d) {
        posn-=4; for (i=posn; i<posn+4; i++) line[i+4]=line[i]; 
      }
      line[posn++]=x-g.x0+32; line[posn++]=y-g.y0+32;
      line[posn++]=r; line[posn++]=d; 
      count+=4;
    }  
  }
  // update to current destination:
  x1=g.x0+line[dest]-32; y1=g.y0+line[dest+1]-32;
  r1=line[dest+2]; d=line[dest+3];
}

//---------------------------------------------------------------
// draw local map screen
void local_screen()
{
  byte i,n,x,y;
  display_init(1);
  
  for (x=0; x<64; x++) {
     for (i=0; i<8; i++) col[i]=0; // vertical 64-bit buffer
     // scan through line[] (closest candidates) and plot them
     for (i=0; i<count; i+=4) if (i!=dest && line[i]==x) {
        y=line[i+1];
        col[y/8]|=(1<<(y%8));
     }
     if (x==32) { col[4]|=3; col[3]|=128;} // add center point
     if (x==31 || x==33) col[4]|=1; // add center point
     for (i=0; i<8; i++) i2c_sendByte(col[i]);
  }
  for (i=0; i<8; i++) i2c_sendByte(255); // vertical line
  display_init(0);
}

//----------------------------------------------------------
#define PERCENT 11
PROGMEM const byte forestdensity[]="Forest`";
int choose_destination()
{
  int i;
 
  while(1) {
    within_range();
    r=r1; tpos=13; name_town();
    write_text();
    tpos=16; add_mileage(d); write_text();
    density=(r1&7)+2; if (density>8) density=8; // upper limit
    tpos=11; p=forestdensity; add_text();
    tpos=19; add_number(density); text[20]=PERCENT;
    for (i=0; i<6; i++) write_text(); // padding
    within_range(); // since doing the text wiped line[]
    while(buttons()==0) { 
      local_screen();
      _delay_ms(100);
      dest^=128;  // toggle top bit - for the flash effet
    }
    dest&=127;
    i=buttons_db();
    if (i==LEFT_BUTTON) { dest+=4; if (dest==count) dest=0;}
    if (i==RIGHT_BUTTON) {
       x=x1; y=y1;  prices(LIST);
       if (buttons_db()==RIGHT_BUTTON) return(0);
    }
  } 
}


//---------------------------------------------------------------
void map_island()
{   
   m=11; w=11; // toggles
  
   while(buttons()==0) {
     for (n=0; n<8; n++) {
       for (i=0; i<128; i++) line[i]=0;
       r=0;
       x=0; y=0;
       for (i=0; i<255; i++) {       
         y+=r;  xorshift(); x+=r;
         if (y/32==n && r!=m && r!=w)  line[x/2]|=(1<<((y%32)/4));
       }
       write_line();
     }
     if (m==g.r0) { m=11;  if (w==g.job) w=11; else w=g.job; }
     else m=g.r0; // fast toggle current location
     _delay_ms(100);
   }
   buttons_db();
}


//------------------------------------------------------------
#define COLON 47
PROGMEM const byte morn[]="Morning";
PROGMEM const byte night[]="Night";

PROGMEM const byte at[]="at_";
PROGMEM const byte needed[]="_needed";
PROGMEM const byte STATUS[]="\
_[[_STATUS_REPORT_[[_\0\
Day\0\
At`_\0\
Cash`\0\
Fuel`\0\
Cargo`_\\___Health`\0\
Job_\0\
______";
void status()
{
  const byte *p0;
  display_init(0); // just to get back to (0,0)
  
  p0=STATUS;
  
  for (i=0; i<8; i++) {
    p=p0; add_text(); p0=p;
    if (i==1) {tpos=5; add_number(g.day);text[6]=','-32; tpos=8;
               if (g.sun) p=morn; else p=night; add_text();}
    if (i==2) {r=g.r0; name_town(); }
    if (i==3) {tpos=12; add_cash(); text[tpos]=DOLLAR;}
    if (i==4) {tpos=10; add_mileage(g.cargo[FUEL]);}
    if (i==5) {text[6]=g.curcargo;  text[8]=g.cargo[MAXCARGO];
              text[18]=g.cargo[HEALTH];}
	   
    if (i==6) { add_number(g.job-71);  text[5]=COLON;
               p=m_items; w=g.job&7;
	       while(w!=0) {while(pgm_read_byte(p)!=0) p++; p++; w--;}
             tpos=6; add_text(); p=needed; add_text();}
    if (i==7) { p=at;add_text(); r=g.job; name_town();}
    write_text();
  }
  buttons_db();
}


//-------------------------------------------------------------
PROGMEM const byte menu[]="\
_Status_Report\0\
_Local_Prices\0\
_Buy\\Equip\0\
_Sell\0\
_Island_Map\0\
_Choose_Destination\0\
_Depart_for_\0\
_Off";

int main_menu()
{
  i=0;
  while(1) {
    r=r1;
    display_init(0); // reset display
    tpos=0;
    p=menu;
    for (n=0; n<8; n++) {   
      add_text();
      if (n==6) {r=r1; name_town();}
      if (n==i) text[0]=44;
      write_text();
    }
    n=buttons_db();
    if (n==LEFT_BUTTON) {i++; if (i==8) i=0;} // else return(i);
    if (n==RIGHT_BUTTON) return(i);
  }
}  


//--------------------------------------------------------
int main()
{
  int i,n;
  int posn=0;

  core_init();
  initial_state();
  load_state();
 
  display_init(0);
 
  within_range(); // set up closest town
  status();

  while(1) {
    i=main_menu();
    x=g.x0; y=g.y0;
    if (i==0) status();
    if (i==1) {prices(LIST); buttons_db();}
    if (i==2) prices(BUY);
    if (i==3) prices(SELL);
    if (i==4) map_island();
    if (i==5) choose_destination();
    if (i==6 && d!=0) {journey(); status(); dest=0; within_range(); }
    if (i==7) powerDown(0);
  }
}
