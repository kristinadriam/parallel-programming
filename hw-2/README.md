# отчет

## задача 1

### идея

сделаем протокол «запрос–разрешение»: никто не уходит, пока второй явно не подтвердил, что он на посту.

### сообщения

- `REQUEST_TO_LEAVE` = «хочу уйти на обед, можно?»
- `OK` = «да, я сейчас смотрю, уходи»

### алгоритм

опищем алгорим для Пети, для Васи будет симметричным.

1. Петя захотел обедать → продолжает смотреть, шлет Васе `REQUEST_TO_LEAVE`.
2. Вася, получив `REQUEST_TO_LEAVE`:
   - Вася не обедает → он отвечает `OK`.
   - если Вася тоже одновременно запрашивает `REQUEST_TO_LEAVE`, то  нужно как-то не зависнуть:
     - зафиксируем приоритет (например, у Пети приоритет выше).
     - низший приоритет при конфликте отвечает `OK`, а высший ждет.
3. Петя уходит есть только после получения `OK`.
4. при возвращении никакого сообщения не нужно, так как тот, кто хочет уйти, все равно будет ждать `OK`.

#### гарантия непрерывного наблюдения

пока Петя не получил `OK`, он не может уйти, значит реактор под наблюдением.

если Петя получил `OK`, это означает, что Вася в этот момент гарантирует, что он смотрит, значит когда Петя уйдет, Вася остается.

#### минимум сообщений

минимум 2 сообщения на один уход на обед: `REQUEST_TO_LEAVE` + `OK`.

gочему нельзя 1 сообщение: если оба одновременно сообщат, что ушли, то из‑за задержек доставки оба могут уйти, и наблюдение прервется. в асинхронной модели без часов нужно подтверждение.

если оба должны пообедать по разу → 4 сообщения (по 2 на каждого).

---

## задача 2

```py
for i in range(100):
    temp = count
    count = temp + 1
```

100 потоков → всего 100 * 100 = 10000 «инкрементов», но они гоняются.

### максимальное возможное `count`

**максимум = 10000.**

возможно, если планировщик так удачно все перемешивает, что каждый read видит актуальный `count` и ни одно обновление не теряется (как будто инкремент атомарный).

### минимальное возможное `count`

**минимум = 100.**

*rак получить 100*: делаем «волны» по итерациям.

- пусть перед волной `count = v`.
- все 100 потоков сначала выполняют `temp = count` и читают одно и то же `v`.
- потом все 100 потоков выполняют `count = temp + 1` и записывают одно и то же `v+1`.

итог «волны»: `count` вырос только на 1, хотя записей было 100.

таких волн 100 (по числу итераций) → итог `count = 100`.

### вывод

*почему меньше 100 нельзя*:

чтобы поток закончил свою $k$-ю итерацию, он обязан выполнить запись `count = temp + 1`, а значение `temp` он прочитал после завершения $(k−1)$-й итерации, то есть оно не может соответствовать состоянию «меньше чем $k−1$». в результате после того как все потоки завершили $k$ итераций, `count` уже не может оказаться меньше $k$. в конце $k=100$ → `count` $\geq 100$.

**итоговый диапазон:** $100 \leq$ `count` $\leq 10000$.

---

## задача 3.1

сделаем через condition и счетчики ожидания:

```py
import threading

lock = threading.Lock()
lead_cv = threading.Condition(lock)
follow_cv = threading.Condition(lock)

waiting_leads = 0
waiting_follows = 0

def leader():
    global waiting_leads, waiting_follows
    with lock:
        if waiting_follows > 0:
            waiting_follows -= 1
            follow_cv.notify()   # разбудили одного ведомого
        else:
            waiting_leads += 1
            lead_cv.wait()       # ждем ведомого
    dance()

def follower():
    global waiting_leads, waiting_follows
    with lock:
        if waiting_leads > 0:
            waiting_leads -= 1
            lead_cv.notify()     # разбудили одного ведущего
        else:
            waiting_follows += 1
            follow_cv.wait()     # ждем ведущего
    dance()
```

здесь ведущий/ведомый просто кто-то — не гарантируется конкретная пара, то есть кто выбрал кого.

---

## задача 3.2

нужно реально сматчить конкретные потоки и синхронизировать старт пары.

будем хранить в очереди ожидания объект ожидания (`Waiter`) `с`:

- `matched` — сигнал «тебя выбрали»
- `partner_id`
- `Barrier(2)` — чтобы оба стартанули вместе

```py
import threading
from collections import deque

class Waiter:
    def __init__(self, my_id):
        self.my_id = my_id
        self.partner_id = None
        self.matched = threading.Event()
        self.barrier = threading.Barrier(2)

lock = threading.Lock()
waiting_leaders = deque()
waiting_followers = deque()

def leader(my_id):
    with lock:
        if waiting_followers:
            f = waiting_followers.popleft()
            f.partner_id = my_id
            partner_id = f.my_id
            b = f.barrier
            f.matched.set()
            me = None
        else:
            me = Waiter(my_id)
            waiting_leaders.append(me)
            partner_id = None
            b = None

    if partner_id is None:
        me.matched.wait()
        partner_id = me.partner_id
        b = me.barrier

    b.wait()
    dance(partner_id)

def follower(my_id):
    with lock:
        if waiting_leaders:
            l = waiting_leaders.popleft()
            l.partner_id = my_id
            partner_id = l.my_id
            b = l.barrier
            l.matched.set()
            me = None
        else:
            me = Waiter(my_id)
            waiting_followers.append(me)
            partner_id = None
            b = None

    if partner_id is None:
        me.matched.wait()
        partner_id = me.partner_id
        b = me.barrier

    b.wait()
    dance(partner_id)
```

- пара фиксируется явно через `partner_id` в конкретном объекте ожидания.
- никакой другой поток не сможет украсть этого партнера, т.к. объект уже вынули из очереди.
- `Barrier(2)` гарантирует, что оба дошли до одной точки и начнут `dance()` синхронно.
