#define F_CPU 16000000UL
#define Start 1
#define Stop  0

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned char Data_rx;
unsigned int N_takt = 0;
unsigned int timer_Start_Stop(char flag);
unsigned int counter1 = 0;

ISR(USART_RXC_vect) {
    Data_rx = UDR;
}

ISR(INT0_vect) {

    if (PIND & (1 << 2)) {                  // если передний фронт
        timer_Start_Stop(Start);            // запускаем таймер
    }
    else {                                  // иначе - задний фронт
        N_takt = timer_Start_Stop(Stop);    // останавливаем таймер
    }
}

ISR(TIMER2_OVF_vect) {
    counter1++;
}

void Port_init(void) {

    DDRD |= (1 << 3);       // trig
    PORTD &= ~(1 << 3);

    DDRD &= ~(1 << 2);      // echo
    PORTD &= ~(1 << 2);

    DDRD |= (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);
    PORTD &= ~((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));

    DDRB = 0b11111111;
    PORTB = 0b11111111;
}

void timer_init(void) {

    // таймер 0 - шим сигнал
    // таймер 1 - датчик расстояния
    // таймер 2 - запуск датчика расстояния по переполнению

    TCCR0 |= (1 << WGM00) | (1 << WGM01);   // устанавливаем режим шим таймера 0
    TCCR0 |= (1 << COM01);                  // определяем механизм изм сост ножки тамера 0
    OCR0 = 255;                             // определяем число сравнения таймера 0

    TIMSK |= (1 << TOIE2);                  // разрешаем уход в прерывание по переполнению счетного регистра таймера 2

    TCCR0 |= (1 << CS00);                   // запуск таймера без предделителя с этого момента таймер 0 начал работать
    TCCR2 |= (1 << CS02) | (1 << CS02);     // запуск таймера c предделителем 1024 с этого момента таймер 2 начал работать
}

void usart_init(void) {

    UBRRH = 0;
    UBRRL = 103; // 9600 при 16MHz
    UCSRB |= (1 << RXEN) | (1 << RXCIE) | (1 << TXEN);      // RXCIE - разрешение прерывания по приему, RXEN - разрешение работы преимника
    UCSRC |= (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);    // 8-бит данных, 1 стоп бит, бита четности нет
}

void ext_interrupt_init(void) {
    GICR |= (1 << INT0);    // разрешение прерывания на INT0
    MCUCR |= (1 << ISC00);  // Прерывание по любому фронту
}

void uart_send_char(unsigned char c) {
    while (!(UCSRA & (1 << UDRE)));
    UDR = c;
}

void uart_send_string(char char_array[]) {
    int i = 0;
    while (char_array[i] != 0) {
        uart_send_char(char_array[i]);
        i++;
    }
}

void HC_SRC4_Send_strob(void) { // отправляем 20мкс Строб
    PORTD |= (1 << 3);          // устанавливаем ЛОГ 1
    _delay_us(20);              // длительность импульса
    PORTD &= ~(1 << 3);         // устанавливаем ЛОГ 0
}

unsigned int timer_Start_Stop(char flag) {  // функция Запуска и Остановки

    unsigned int rez;           // для хранения тактов	
    if (flag) {
        TCNT1H = 0;             // обнуляем
        TCNT1L = 0;
        TCCR1B |= (1 << CS11);  // запуск таймера с предделителем 8
        //PORTB=0b11111111;
        return 0;
    }
    else {
        TCCR1B &= ~(1 << CS11); // останавливаем таймер
        rez = TCNT1;
        //PORTB=0b00000000;
        return rez;
    }
}

unsigned int   HC_SRC4_Convert_CM(unsigned int N_t) {
    unsigned long int C;
    C = N_t / 2;                // определяем время импульса
    C /= 58;                    // вычисляем расстояние в сантиметрах
    return (unsigned int)C;
}

unsigned int   HC_SRC4_Get_CM(void) {
    HC_SRC4_Send_strob();					// отправляем строб
    //_delay_ms(10);					
    return   HC_SRC4_Convert_CM(N_takt);	// возвращаем значение в СМ
}

void init_adc() {
    ADMUX = 0b00000000;                                     // выбираем источник питания АЦП 5v aref
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);   // устанавливаем предделитель в 128
    ADCSRA |= (1 << ADEN);                                  // включаем АЦП
}

unsigned int read_adc(void) {
    ADMUX |= 0;                             // 0-я ножка
    _delay_us(10);                          // задержка на установление опорного напряжения
    ADCSRA |= (1 << ADSC);                  // запуск преобразования АЦП
    while ((ADCSRA & (1 << ADIF)) == 0);    // ожидание окончания преобразования
    return ADCW;
}

void StopMotors() {
    PORTD &= ~((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
}


int main(void) {

    Port_init();
    timer_init();
    usart_init();
    ext_interrupt_init();
    init_adc();

    sei();  // разрешаем прерывание

    while (1) {
        if (counter1 == 64) {                       // типа предделитель

            TCCR2 &= ~((1 << CS02) | (1 << CS02));  // останавливаем таймер				
            unsigned int x = HC_SRC4_Get_CM();
            if (x < 10) {
                char num[8];
                itoa(x, num, 10);
                strcat(num, "\r");
                uart_send_string(num);
                StopMotors();
            }
            counter1 = 0;
            TCCR2 |= (1 << CS02) | (1 << CS02);     // снова запускаем таймер				
        }
        if (Data_rx == 'm') {
            Data_rx = 0;
            char char_array[] = { "M00" };
            uart_send_string(char_array);
            StopMotors();
            PORTD |= (1 << 4) | (1 << 6);   // move forward
        }
        if (Data_rx == 's') {
            Data_rx = 0;
            char char_array[] = { "S00" };
            uart_send_string(char_array);
            StopMotors();                   // stop motors
        }
        if (Data_rx == 'l') {
            Data_rx = 0;
            char char_array[] = { "L00" };
            uart_send_string(char_array);
            StopMotors();
            //_delay_ms(100);
            PORTD |= (1 << 5) | (1 << 6);   // move left
        }
        if (Data_rx == 'r') {
            Data_rx = 0;
            char char_array[] = { "R00" };
            uart_send_string(char_array);
            StopMotors();
            //_delay_ms(100);
            PORTD |= (1 << 4) | (1 << 7);   // move right
        }
        if (Data_rx == 'v') {
            Data_rx = 0;
            char num[8];
            sprintf(num, "%4.2f", read_adc()*0.0048828);    // check accum
            uart_send_string(num);
        }
        if (Data_rx == '1' || Data_rx == '2' || Data_rx == '3') { // set speed 33%, 66%, 100%

            if (Data_rx == '1') {
                Data_rx = 0;
                OCR0 = 85;
                uart_send_string("33p");
            }
            if (Data_rx == '2') {
                Data_rx = 0;
                OCR0 = 170;
                uart_send_string("66p");
            }
            if (Data_rx == '3') {
                Data_rx = 0;
                OCR0 = 255;
                uart_send_string("100p");
            }
        }
    }
}