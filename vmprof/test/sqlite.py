import sqlite3
import math
from os import unlink
from os.path import exists


def main():
    if exists('test.db'):
        unlink('test.db')
    conn = sqlite3.connect('test.db')

    c = conn.cursor()
    c.execute("""
CREATE TABLE stocks
(date text, trans text, symbol text, qty real, price real)
""")
    for i in range(10000):
        c.execute("""
        INSERT INTO stocks VALUES ('{}','{}','{}', {}, {})
        """.format('2011-01-05', 'BUY', 'ABC', i, math.sqrt(i)))


main()
