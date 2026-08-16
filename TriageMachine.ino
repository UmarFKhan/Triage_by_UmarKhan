/*
  ============================================================================
  Project      : Emergency Triage Management System (Hardware Simulation)
  Subject      : Data Structures and Algorithms (DSA)
  Description  : This project demonstrates a real-world, hardware-based
                 implementation of the Queue data structure. It simulates
                 a hospital emergency triage system, where incoming patients
                 are categorized into priority levels (RED, YELLOW, GREEN,
                 and DEAD) and served in First-In-First-Out (FIFO) order
                 within each priority category.

                 The system uses a 4x4 matrix keypad for user input, a
                 20x4 I2C LCD for status display, a buzzer for audio
                 feedback, and status LEDs to indicate which queues are
                 currently active.

  Authors      : Umar Khan (Lead Developer)
                 Huzaifa Masood (Assisted with Classroom Presentation)

  Note         : This sketch was created as the Final Project submission
                 for the DSA course. Inline comments are provided
                 throughout the code to guide readers through the logic
                 and hardware interfacing.
  ============================================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ============================================================================
// LCD CONFIGURATION
// ============================================================================
// I2C address is commonly 0x27 or 0x3F depending on the LCD backpack module.
// Display size: 20 columns x 4 rows.
LiquidCrystal_I2C lcd(0x27, 20, 4);   // change to 0x3F if the screen stays blank

// ============================================================================
// BUZZER CONFIGURATION
// ============================================================================
// Used to give audible feedback for keypresses, confirmations, and errors.
const int BUZZER_PIN = 2;

// ============================================================================
// STATUS LED CONFIGURATION
// ============================================================================
// Each LED lights up whenever its corresponding queue contains at least
// one patient, giving an at-a-glance view of active triage categories.
// A0 = RED, A1 = YELLOW, A2 = GREEN, A3 = DEAD
const int LED_RED    = A0;
const int LED_YELLOW = A1;
const int LED_GREEN  = A2;
const int LED_DEAD   = A3;

// IMPORTANT: Some DEAD-status LEDs are wired active-LOW (they light up when
// the pin is LOW instead of HIGH). Set this flag to match your wiring.
// If your DEAD LED stays ON by default and turns OFF once a patient is
// registered, keep this set to false.
const bool DEAD_LED_ACTIVE_LOW = false;

// ============================================================================
// KEYPAD CONFIGURATION
// ============================================================================
const byte ROWS = 4;
const byte COLS = 4;

// Physical key layout, verified against the wired keypad module.
char keys[ROWS][COLS] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

// Digital pins connected to the keypad's row and column lines.
byte rowPins[ROWS] = {3, 4, 5, 6};
byte colPins[COLS] = {7, 8, 9, 10};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============================================================================
// QUEUE DATA STRUCTURE
// ============================================================================
// Core DSA concept: a circular queue implemented using a fixed-size array,
// with head/tail pointers and a count to track the number of stored items.
// Each queue holds Medical Record (MR) numbers for patients in that
// priority category, served strictly in FIFO order.
const int MAXQ = 20;   // maximum patients per priority queue

struct Queue {
  long data[MAXQ];   // circular buffer storing MR numbers
  int head;          // index of the front element (next to be served)
  int tail;          // index where the next element will be inserted
  int count;         // current number of elements in the queue
};

// One queue per triage priority level.
Queue qRed, qYellow, qGreen, qDead;

// ============================================================================
// AUDIO FEEDBACK HELPERS
// ============================================================================

// Sounds the buzzer for a given duration (in milliseconds).
void beep(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// Plays a distinct beep pattern depending on triage level, so the operator
// gets audible confirmation of which category was just used.
void beepPattern(int lvl) {
  if (lvl == 1) { beep(80); delay(60); beep(80); delay(60); beep(80); }   // RED    -> 3 short beeps
  else if (lvl == 2) { beep(120); delay(80); beep(120); }                 // YELLOW -> 2 medium beeps
  else if (lvl == 3) { beep(150); }                                       // GREEN  -> 1 beep
  else if (lvl == 4) { beep(300); delay(120); beep(300); }                // DEAD   -> 2 long beeps
}

// ============================================================================
// QUEUE OPERATIONS (CORE DSA LOGIC)
// ============================================================================

// Resets a queue to its empty state.
void qInit(Queue &q) { q.head = q.tail = q.count = 0; }

// Enqueue: inserts an MR number at the tail of the queue.
// Returns false if the queue is already full (overflow condition).
bool qPush(Queue &q, long mr) {
  if (q.count >= MAXQ) return false;
  q.data[q.tail] = mr;
  q.tail = (q.tail + 1) % MAXQ;   // wrap around for circular buffer behavior
  q.count++;
  return true;
}

// Dequeue: removes and returns the MR number at the head of the queue.
// Returns false if the queue is empty (underflow condition).
bool qPop(Queue &q, long &mr) {
  if (q.count == 0) return false;
  mr = q.data[q.head];
  q.head = (q.head + 1) % MAXQ;   // wrap around for circular buffer behavior
  q.count--;
  return true;
}

// Peek: returns the MR number at the head of the queue without removing it.
// Returns -1 if the queue is empty.
long qPeek(Queue &q) {
  if (q.count == 0) return -1;
  return q.data[q.head];
}

// Returns the total number of patients currently registered across
// all four priority queues.
int totalPatients() {
  return qRed.count + qYellow.count + qGreen.count + qDead.count;
}

// ============================================================================
// LED STATUS INDICATORS
// ============================================================================

// Writes the DEAD LED state while accounting for active-HIGH vs active-LOW wiring.
void writeDeadLed(bool on) {
  if (!DEAD_LED_ACTIVE_LOW) {
    digitalWrite(LED_DEAD, on ? HIGH : LOW);
  } else {
    digitalWrite(LED_DEAD, on ? LOW : HIGH);
  }
}

// Refreshes all four status LEDs based on whether each queue currently
// has one or more patients waiting.
void updatePhaseLEDs() {
  digitalWrite(LED_RED,    (qRed.count    > 0) ? HIGH : LOW);
  digitalWrite(LED_YELLOW, (qYellow.count > 0) ? HIGH : LOW);
  digitalWrite(LED_GREEN,  (qGreen.count  > 0) ? HIGH : LOW);
  writeDeadLed(qDead.count > 0);
}

// ============================================================================
// LCD SCREENS
// ============================================================================

// Main menu screen showing available actions and the total patient count.
void showHome() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("TRIAGE MACHINE");
  lcd.setCursor(0,1); lcd.print("A:Add  B:Serve");
  lcd.setCursor(0,2); lcd.print("C:View D:Clear");
  lcd.setCursor(0,3); lcd.print("Total: ");
  lcd.print(totalPatients());
}

// Displays a live summary of all queue counts and previews the next
// patient in line, following priority order (RED > YELLOW > GREEN > DEAD).
void showCounts() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("R:"); lcd.print(qRed.count);
  lcd.print(" Y:"); lcd.print(qYellow.count);
  lcd.print(" G:"); lcd.print(qGreen.count);
  lcd.print(" D:"); lcd.print(qDead.count);

  lcd.setCursor(0,2);
  long mr = -1;
  // Check queues in priority order to find the next patient to be served.
  if (qRed.count)        { lcd.print("Next RED  MR "); mr = qPeek(qRed); }
  else if (qYellow.count){ lcd.print("Next YEL  MR "); mr = qPeek(qYellow); }
  else if (qGreen.count) { lcd.print("Next GRN  MR "); mr = qPeek(qGreen); }
  else if (qDead.count)  { lcd.print("Next DEAD MR "); mr = qPeek(qDead); }
  else                   { lcd.print("No patients"); }

  if (mr != -1) {
    lcd.setCursor(0,3);
    lcd.print("MR: "); lcd.print(mr);
  } else {
    lcd.setCursor(0,3);
    lcd.print("* to back");
  }
}

// ============================================================================
// INPUT FLOWS
// ============================================================================

// Prompts the operator to key in a patient's Medical Record (MR) number.
// '#' confirms entry, '*' cancels and returns to the previous screen.
// Returns the entered MR number, or -1 if cancelled.
long enterMR() {
  String mr = "";
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Enter MR Number");
  lcd.setCursor(0,1); lcd.print("#=OK  *=Cancel");
  lcd.setCursor(0,3); lcd.print("MR: ");

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;   // no key pressed this cycle, keep polling

    if (k >= '0' && k <= '9') {
      // Append digit, capped at 10 digits to avoid overflow/display issues.
      if (mr.length() < 10) {
        mr += k;
        lcd.setCursor(4,3); lcd.print("                ");
        lcd.setCursor(4,3); lcd.print(mr);
        beep(35);
      }
    } else if (k == '*') {
      beep(120);
      return -1;   // cancelled by operator
    } else if (k == '#') {
      if (mr.length() == 0) { beep(200); continue; }   // reject empty entry
      beep(80);
      return mr.toInt();
    } else {
      beep(150);   // invalid key for this context
    }
  }
}

// Prompts the operator to classify the patient into one of the four
// triage levels using keys A/B/C/D. Returns -1 if cancelled.
int selectTriage() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Select Triage");
  lcd.setCursor(0,1); lcd.print("A=RED B=YEL");
  lcd.setCursor(0,2); lcd.print("C=GRN D=DEAD");
  lcd.setCursor(0,3); lcd.print("*=Cancel");

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k == '*') return -1;
    if (k == 'A') { beep(60); return 1; }   // RED    -> highest priority
    if (k == 'B') { beep(60); return 2; }   // YELLOW -> medium priority
    if (k == 'C') { beep(60); return 3; }   // GREEN  -> low priority
    if (k == 'D') { beep(60); return 4; }   // DEAD   -> lowest priority
    beep(150);   // invalid key
  }
}

// ============================================================================
// MAIN OPERATIONS
// ============================================================================

// Registers a new patient: collects MR number and triage level, then
// enqueues the patient into the corresponding priority queue.
void addPatientFlow() {
  long mr = enterMR();
  if (mr < 0) { showHome(); return; }   // operator cancelled

  int lvl = selectTriage();
  if (lvl < 0) { showHome(); return; }  // operator cancelled

  // Enqueue into the queue matching the selected priority level.
  bool ok = false;
  if (lvl == 1) ok = qPush(qRed, mr);
  else if (lvl == 2) ok = qPush(qYellow, mr);
  else if (lvl == 3) ok = qPush(qGreen, mr);
  else if (lvl == 4) ok = qPush(qDead, mr);

  lcd.clear();
  if (!ok) {
    // Queue overflow: the selected priority category is already full.
    lcd.setCursor(0,0); lcd.print("Queue Full!");
    beep(300);
    delay(1000);
    showHome();
    return;
  }

  updatePhaseLEDs();
  beepPattern(lvl);

  lcd.setCursor(0,0); lcd.print("Added MR "); lcd.print(mr);
  lcd.setCursor(0,1); lcd.print("Phase: ");
  if (lvl == 1) lcd.print("RED");
  if (lvl == 2) lcd.print("YELLOW");
  if (lvl == 3) lcd.print("GREEN");
  if (lvl == 4) lcd.print("DEAD");
  delay(1200);

  showHome();
}

// Serves (dequeues) the next patient, always respecting strict priority
// order: RED is served before YELLOW, YELLOW before GREEN, and so on.
void serveNext() {
  long mr; int lvl = 0;

  // Priority-order dequeue: only move to the next category if the
  // higher-priority queue is empty.
  if (qRed.count && qPop(qRed, mr)) lvl = 1;
  else if (qYellow.count && qPop(qYellow, mr)) lvl = 2;
  else if (qGreen.count && qPop(qGreen, mr)) lvl = 3;
  else if (qDead.count && qPop(qDead, mr)) lvl = 4;

  lcd.clear();
  if (lvl == 0) {
    // All queues are empty, nothing to serve (underflow condition).
    lcd.setCursor(0,0); lcd.print("No patients");
    beep(200);
    delay(800);
    showHome();
    return;
  }

  updatePhaseLEDs();
  beepPattern(lvl);

  lcd.setCursor(0,0); lcd.print("Serving MR ");
  lcd.print(mr);
  lcd.setCursor(0,1); lcd.print("Phase: ");
  if (lvl == 1) lcd.print("RED");
  if (lvl == 2) lcd.print("YELLOW");
  if (lvl == 3) lcd.print("GREEN");
  if (lvl == 4) lcd.print("DEAD");

  delay(1300);
  showHome();
}

// Clears all four queues after operator confirmation. Used to reset
// the system, e.g. at the start of a new shift or demonstration.
void clearAll() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Clear all queues?");
  lcd.setCursor(0,1); lcd.print("#=Yes  *=No");

  while (true) {
    char k = keypad.getKey();
    if (!k) continue;

    if (k == '*') { showHome(); return; }   // cancelled
    if (k == '#') {
      // Confirmed: reinitialize every queue to empty.
      qInit(qRed); qInit(qYellow); qInit(qGreen); qInit(qDead);
      updatePhaseLEDs();
      beep(200); delay(100); beep(200);
      lcd.setCursor(0,3); lcd.print("Cleared");
      delay(800);
      showHome();
      return;
    }
    beep(150);   // invalid key
  }
}

// ============================================================================
// SETUP & MAIN LOOP
// ============================================================================

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_DEAD, OUTPUT);

  // Initialize all four priority queues to an empty state.
  qInit(qRed); qInit(qYellow); qInit(qGreen); qInit(qDead);

  lcd.init();
  lcd.backlight();

  // Ensure LEDs reflect the (empty) starting state correctly.
  updatePhaseLEDs();

  beep(100); delay(80); beep(100);   // startup confirmation beep
  showHome();
}

void loop() {
  char k = keypad.getKey();
  if (!k) return;   // no key pressed, keep polling

  if (k == 'A') addPatientFlow();       // Add a new patient
  else if (k == 'B') serveNext();       // Serve the next patient by priority
  else if (k == 'C') {                  // View current queue status
    showCounts();
    while (true) {
      char x = keypad.getKey();
      if (x == '*') { showHome(); break; }   // '*' returns to main menu
    }
  }
  else if (k == 'D') clearAll();        // Clear all queues
}
