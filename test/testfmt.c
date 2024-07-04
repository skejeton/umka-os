#include "fmt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
  // String read stream

  char dest[13] = {0};
  RWStream rw1 = rwStreamMkStr("Hello world!");
  assert(rw1.read(&rw1, dest, 5) == 5);
  assert(strcmp(dest, "Hello") == 0);
  assert(rw1.read(&rw1, dest, 1) == 1);
  assert(rw1.read(&rw1, dest, 2344572) == 6);
  assert(strcmp(dest, "world!") == 0);
  assert(rw1.read(&rw1, dest, 123) == 0);
  printf("OK READ\n");

  // String write stream

  RWStream rw2 = rwStreamMkStrS(dest, 13);
  assert(rw2.write(&rw2, "Hello", 5) == 5);
  assert(strcmp(dest, "Hello") == 0);
  assert(rw2.write(&rw2, " ", 1) == 1);
  assert(rw2.write(&rw2, "world!", 6) == 6);
  assert(rw2.write(&rw2, "gjfalskdfjl", 11) == 0);
  assert(strcmp(dest, "Hello world!") == 0);
  printf("OK WRITE\n");

  // Format functions

  rw1 = rwStreamMkStr("123 4A6 666 010101");

  assert(fmtParseULL(&rw1, 10) == 123);
  assert(fmtParseULL(&rw1, 16) == 0x4A6);
  assert(fmtParseULL(&rw1, 8) == 0666);
  assert(fmtParseULL(&rw1, 2) == 0b010101);

  rw1 = rwStreamMkStr("123 0x4A6 0666 0X123F");

  assert(fmtParseULL(&rw1, 0) == 123);
  assert(fmtParseULL(&rw1, 0) == 0x4A6);
  assert(fmtParseULL(&rw1, 0) == 0666);
  assert(fmtParseULL(&rw1, 0) == 0x123F);

  printf("OK ULL\n");

  rw1 = rwStreamMkStr("-534 324 324 -ABC +10101");
  assert(fmtParseLL(&rw1, 10) == -534);
  assert(fmtParseLL(&rw1, 10) == 324);
  assert(fmtParseLL(&rw1, 16) == 0x324);
  assert(fmtParseLL(&rw1, 16) == -0xABC);
  assert(fmtParseLL(&rw1, 2) == 0b10101);

  rw1 = rwStreamMkStr("-534 +324 0x324 -0xABC");
  assert(fmtParseLL(&rw1, 0) == -534);
  assert(fmtParseLL(&rw1, 0) == 324);
  assert(fmtParseLL(&rw1, 0) == 0x324);
  assert(fmtParseLL(&rw1, 0) == -0xABC);

  printf("OK LL\n");

  rw1 = rwStreamMkStr("  3.14159   -0.1234   1.0e3  0.1   0.0001  -0.0001e010  "
                      "0e200  1e-4  +0001e+0 0000012e-0 0.0");
  printf("exp 3.14159 got %g\n", fmtParseD(&rw1));
  printf("exp -0.1234 got %g\n", fmtParseD(&rw1));
  printf("exp 1000 got %g\n", fmtParseD(&rw1));
  printf("exp 0.1 got %g\n", fmtParseD(&rw1));
  printf("exp 0.0001 got %g\n", fmtParseD(&rw1));
  printf("exp -1e+6 got %g\n", -fmtParseD(&rw1));
  printf("exp 0 got %g\n", fmtParseD(&rw1));
  printf("exp 0.0001 got %g\n", fmtParseD(&rw1));
  printf("exp 1 got %g\n", fmtParseD(&rw1));
  printf("exp 12 got %g\n", fmtParseD(&rw1));
  printf("exp 0 got %g\n", fmtParseD(&rw1));

  printf("CHECK VALUES\n");

  // Test scanf
  rw1 = rwStreamMkStr("HELLOWORLD    \t  -1234");
  char str[100] = {0};
  int num = 0;
  int x = 0;

  assert(fmtscanf(&rw1, "%s %d", str, &num) == 2);
  assert(strcmp(str, "HELLOWORLD") == 0);
  assert(num == -1234);
  rw1 = rwStreamMkStr("HELLOWORLD    \t  JDFKDJF");
  assert(fmtscanf(&rw1, "%s %d", str, &num) == 1);
  rw1 = rwStreamMkStr("0xDEADBEEF");
  assert(fmtscanf(&rw1, "0x%x%n", &x, &num) == 2);
  assert(x == 0xDEADBEEF);
  assert(num == 10);
  rw1 = rwStreamMkStr(
      "% -0x012345,   +45321, 23421, 666 ABCD123 | 0.123 : A1234 NiceString");

  int p1 = 0;
  int p2 = 0;
  int p3 = 0;
  int p4 = 0;
  int p5 = 0;
  double p6 = 0;
  char p7 = 0;
  void *p8 = 0;
  char p9[100] = {0};

  int cnt = 0;
  assert((cnt = fmtscanf(&rw1, "%% %i,%d, %u, %o %x|%g %c %p %s", &p1, &p2, &p3,
                         &p4, &p5, &p6, &p7, &p8, &p9)));

  assert(p1 == -0x012345);
  assert(p2 == 45321);
  assert(p3 == 23421);
  assert(p4 == 0666);
  assert(p5 == 0xABCD123);
  assert(p6 == 0.123);
  assert(p7 == ':');
  assert(p8 == (void *)0xA1234);
  assert(strcmp(p9, "NiceString") == 0);

  printf("OK SCANF\n");

  // Test number formatting

  char buf[1024];
  rw1 = rwStreamMkStrS(buf, 1024);
  fmtStringifyULL(&rw1, 1235423, 10, true);
  assert(strcmp(buf, "1235423") == 0);
  int p = rw1.tell(&rw1);
  fmtStringifyULL(&rw1, 0x123AB, 16, true);
  assert(strcmp(buf + p, "123AB") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyULL(&rw1, 0561, 8, true);
  assert(strcmp(buf + p, "561") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyULL(&rw1, 0b1110001010, 2, true);
  assert(strcmp(buf + p, "1110001010") == 0);

  p = rw1.tell(&rw1);
  fmtStringifyLL(&rw1, 123, 10, true, false, false);
  assert(strcmp(buf + p, "123") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyLL(&rw1, -123, 10, true, false, false);
  assert(strcmp(buf + p, "-123") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyLL(&rw1, 0xabc, 16, false, true, false);
  assert(strcmp(buf + p, "+abc") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyLL(&rw1, -0666, 8, false, true, false);
  assert(strcmp(buf + p, "-666") == 0);

  p = rw1.tell(&rw1);
  fmtStringifyD(&rw1, 123.456, true, true, false, 2, false);
  assert(strcmp(buf + p, "+123.45") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyD(&rw1, -123.4556, true, true, false, 4, false);
  assert(strcmp(buf + p, "-123.4556") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyD(&rw1, 123.4556, true, false, false, 1, false);
  assert(strcmp(buf + p, "123.4") == 0);
  p = rw1.tell(&rw1);
  fmtStringifyD(&rw1, 123, true, false, false, 3, false);
  assert(strcmp(buf + p, "123.000") == 0);

  printf("OK STRINGIFY\n");

  // Format functions

  rw1 = rwStreamMkStrS(buf, 1024);
  int n = 0;
  fmtprintf(&rw1, "Hello %s %d %x %o %g %c %p %s%n", "world", 123, 0x123, 0123,
            123.456, '!', (void *)0x123, "string", &n);
  assert(strcmp(buf, "Hello world 123 123 123 123.4 ! 0x123 string") == 0);
  assert(n == strlen("Hello world 123 123 123 123.4 ! 0x123 string"));

  // Test flags
  rw1 = rwStreamMkStrS(buf, 1024);
  fmtprintf(&rw1, "%+d %#x %#X %#o %#p %-6d % d % d %04d", 123, 123, 123, 123,
            (void *)0x123, 456, 123, -123, 123);

  // printf("%s\n", buf);
  assert(strcmp(buf, "+123 0x7b 0x7B 0173 0x123 456     123 -123 0123") == 0);

  // Test width
  rw1 = rwStreamMkStrS(buf, 1024);
  fmtprintf(&rw1, "%10d %-10d %6x %-6x %10s %-10s %-5c %5c", 123, 123, 0xABC,
            0xDEF123, "Hello", "world", 'a', 'b');
  // printf("`%10d %-10d %6x %-6x %10s %-10s %-5c %5c`\n", 123, 123, 0xABC,
  //        0xDEF123, "Hello", "world", 'a', 'b');
  // printf("`%s`\n", buf);
  assert(strcmp(buf, "       123 123           abc def123      Hello world     "
                     " a         b") == 0);

  // Test precision
  rw1 = rwStreamMkStrS(buf, 1024);
  fmtprintf(&rw1, "%.3f %.5f %.0f %#-5.8x %.5d %.*s", 123.456, 123.456, 123.456,
            0xC001, 123, 4, "Hello");
  // printf("`%.3f %.5f %.0f %#5.8x %.5d %.*s`\n", 123.456, 123.456, 123.456,
  //        0xC001, 123, 4, "Hello");
  // printf("`%s`\n", buf);
  assert(strcmp(buf, "123.456 123.45600 123 0x0000c001 00123 Hell") == 0);

  printf("OK FORMAT");
}