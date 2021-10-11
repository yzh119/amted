import os
import random
import string

letters = string.ascii_letters
def random_string(length: int) -> str:
    return ''.join(random.choice(letters) for i in range(length))
    
if not os.path.exists('data'):
    os.mkdir('data')

for id in range(1000):
    with open('data/{}.html'.format(id), 'w') as f:
        f.write("<title>{}</title>".format(id))
        f.write("<body>")
        length = random.randint(8192, 1048576)
        f.write(random_string(length))
        f.write("</body>")