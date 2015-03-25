#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef MAX_PATH
#define MAX_PATH 255
#endif // MAX_PATH

long int attempts, backoff, successes, collisions;
double tp, tf, tmax, lmin, lmax, lstep, lambda, s, g, m_clock;
int n, reserved;

//TIPO DELL'EVENTO IN CODA
typedef enum {arrival, transmitted_pack} event_type;

typedef enum {waiting, transmitting, collided} packet_state;

//STRUTTURA CONTENENTE I RISULTATI DA STAMPARE
typedef struct RESULTS {
    long int attempts, backoff, successes, collisions;
    double s, g;
} results;

typedef struct PACKET {
    packet_state state;
} packet;

//DATO RAPPRESENTANTE L'EVENTO IN CODA
typedef struct EVENT {
    double time;
    packet *pack;
    event_type type;
} event;

//ELEMENTO BASE DELLA PRIORITY_LIST
typedef struct QUEUE_ITEM {
    event evt;
    struct QUEUE_ITEM *q_next;
} queue_item;

typedef queue_item *priority_queue;

int compare (event *evt1, event *evt2) {
    if (evt1->time > evt2->time)
        return 1;
    if (evt1->time < evt2->time)
        return -1;
    return 0;
}

packet *createPacket (packet_state state) {
    packet *p = (packet*)malloc (sizeof (packet));
    p->state = state;
    return p;
}

//GENERATORE PSEUDOCASUALE DI NUMERI CON DISTRIBUUZIONE ESPONENZIALE
double randomExp () {
    double x, y;
    while ((x = ((double)rand ()) / ((double)RAND_MAX)) == 0. || x == 1.);
    y = (-1 / lambda) * log (1 - x);
    return y;
}

//CREA L'EVENTO (TIME = CLOCK DEL SISTEMA ATTUALE, TYPE = TIPO DELL'EVENTO)
event createEvent (double time, event_type type, packet *pack) {
    event e;
    switch (type) {
    case arrival:
        e.time = time + randomExp ();
        break;
    case transmitted_pack:
        e.time = time + tf;
        break;
    }
    e.type = type;
    e.pack = pack;
    return e;
}

priority_queue cons (priority_queue queue, event evt) {
    priority_queue q = (priority_queue)malloc (sizeof (queue_item));
    q->evt = evt;
    q->q_next = queue;
    return q;
}

//AGGIUNGE UN ELEMENTO ALLA CODA IN MODO ORDINATO
priority_queue add (priority_queue queue, event evt) {
    if (queue == NULL)
        return cons (queue, evt);
    else if (compare (&evt, &(queue->evt)) <= 0)
        return cons (queue, evt);
    else {
        queue->q_next = add (queue->q_next, evt);
        return queue;
    }
}

//PRENDE UN ELEMENTO DALLA CODA E LO ELIMINA DA QUEST'ULTIMA
priority_queue poll (priority_queue queue, event *evt) {
    (*evt) = queue->evt;
    priority_queue nq = queue->q_next;
    free (queue);
    return nq;
}

//RILASCIA LA MEMORIA OCCUPATA DALLA CODA
priority_queue freeallqueue (priority_queue queue) {
    if (queue != NULL)
        freeallqueue (queue->q_next);
    else return NULL;
    queue->q_next = NULL;
    free (queue->evt.pack);
    free (queue);
    return NULL;
}

void printqueue (priority_queue queue) {
    if (queue != NULL) {
        printf ("Queue item: %lf, %d\n", queue->evt.time, queue->evt.type);
        printqueue (queue->q_next);
    }
}

//FUNZIONE CHIAMATA DALL'EVENTO ARRIVAL
priority_queue onArrival (priority_queue queue, event evt) {
    attempts++;
    evt.pack->state = transmitting;
    queue = add (queue, createEvent (m_clock, transmitted_pack, evt.pack));
    queue = add (queue, createEvent (m_clock, arrival, createPacket (waiting)));
    return queue;
}

//FUNZIONE CHIAMATA DALL'EVENTO TRANSMITTED_PACK
priority_queue onTransmittedPack (priority_queue queue, event evt) {
    priority_queue q = queue;
    while (q != NULL) {
        if (q->evt.pack->state == transmitting) {
            q->evt.pack->state = collided;
            evt.pack->state = collided;
        }
        q = q->q_next;
    }
    if (evt.pack->state == transmitting)
        successes++;
    else {
        collisions++;
        backoff++;
    }
    free (evt.pack);
    return queue;
}

priority_queue onEvent (priority_queue queue, event evt) {
    switch (evt.type) {
    case arrival:
        return onArrival (queue, evt);
    case transmitted_pack:
        return onTransmittedPack (queue, evt);
    }
    return queue;
}

//SIMULAZIONE CON LAMBDA, TP, TF, N, TMAX NOTI
void simulate () {
    int i;
    priority_queue queue = NULL;
    for (i = 0; i < n; i++)
        queue = add (queue, createEvent (m_clock, arrival, createPacket (waiting)));
    while (m_clock <= tmax) {
        event evt;
        queue = poll (queue, &evt);
        m_clock = evt.time;
        queue = onEvent (queue, evt);
    }
    queue = freeallqueue (queue);
}

int main (int argc, char **argv) {
    int i = 0, resc = 0, verbose = 0, p = 0, lastp = 0;
    char filename[MAX_PATH];
    results *resv = NULL;
    results res;
    FILE *f;
    srand (time(NULL));
    for (i = 1; i < argc; i++)
        if (strcmp (argv[i], "-n") == 0)
            n = atof (argv[++i]);
        else if (strcmp (argv[i], "-tf") == 0)
            tf = atof (argv[++i]);
        else if (strcmp (argv[i], "-tp") == 0)
            tp = atof (argv[++i]);
        else if (strcmp (argv[i], "-lmin") == 0)
            lmin = atof (argv[++i]);
        else if (strcmp (argv[i], "-lmax") == 0)
            lmax = atof (argv[++i]);
        else if (strcmp (argv[i], "-lstep") == 0)
            lstep = atof (argv[++i]);
        else if (strcmp (argv[i], "-tmax") == 0)
            tmax = atof (argv[++i]);
        else if (strcmp (argv[i], "-o") == 0)
            strcpy (filename, argv [++i]);
        else if (strcmp (argv[i], "-v") == 0)
            verbose = 1;
    resv = (results*)malloc (sizeof (results) * (int)(lmax - lmin + 1));
    lambda = lmin;
    while (lambda <= lmax) {
        m_clock = 0.;
        attempts = 0;
        collisions = 0;
        backoff = 0;
        successes = 0;
        g = 0.;
        s = 0.;
        reserved = 0;
        simulate ();
        g = (attempts / tmax * tf);
        s = (successes / tmax * tf);
        if (verbose)
            printf ("%d) lambda = %lf, attempts = %ld, backoff = %ld, successes = %ld, collisions = %ld, G = %lf, S = %lf.\n", resc, lambda, attempts, backoff, successes, collisions, g, s);
        else {
            p = (int)(100 * resc / ((lmax - lmin) / lstep));
            if (p % 10 == 0 && p != lastp) {
                printf ("--> %d%s\n", p, "%");
                lastp = p;
            }
        }
        res.attempts = attempts;
        res.backoff = backoff;
        res.collisions = collisions;
        res.successes = successes;
        res.g = g;
        res.s = s;
        resv [resc++] = res;
        lambda += lstep;
    }
    f = fopen (filename, "w+");
    fprintf(f, "ATTEMPTS\tBACKOFF\tSUCCESSES\tCOLLISIONS\tG\tS\n");
    for (i = 0; i < resc; i++)
        fprintf(f, "%ld\t%ld\t%ld\t%ld\t%lf\t%lf\n", resv[i].attempts, resv[i].backoff, resv[i].successes, resv[i].collisions, resv[i].g, resv[i].s);
    fflush (f);
    fclose (f);
    free (resv);
    return 0;
}
