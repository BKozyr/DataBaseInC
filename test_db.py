import subprocess
import os

def run_script(commands):
    if os.path.exists('test.db'):
        os.remove('test.db')

    process = subprocess.Popen(
        ['./main', 'test.db'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True
    )

    input_data = '\n'.join(commands)
    input_data+='\n'

    raw_output, errors = process.communicate(input_data)

    output = raw_output.split('\n')

    return output

def test_inserts_and_retrieves_row():
    result = run_script(["insert 1 user1 person1@example.com", "select", ".exit"])
    expected = [
        "db > Executed.",
        "db > (1, user1, person1@example.com)",
        "Executed.",
        "db > "
        ]
    assert result == expected

def test_prints_error_message_when_table_is_full():
    input=[f"insert {i} user{i} person{i}@example.com" for i in range(0,1401)]
    input.append('.exit')
    result = run_script(input)
    expected = 'db > Error: Table full.'
    assert expected == result[-2]

def test_allows_inserting_strings_that_are_the_maximum_length():
    long_username = "a"*32
    long_email = "a"*255
    script = [
        f"insert 1 {long_username} {long_email}",
        "select",
        ".exit",
    ]
    result = run_script(script)
    expected = [
        "db > Executed.",
        f"db > (1, {long_username}, {long_email})",
        "Executed.",
        "db > ",
        ]
    assert expected == result

def test_prints_error_message_if_strings_are_too_long():
    long_username = "a"*33
    long_email = "a"*256
    script = [
        f"insert 1 {long_username} {long_email}",
        "select",
        ".exit",
    ]
    result = run_script(script)
    expected = [
        "db > String is too long.",
        "db > Executed.",
        "db > ",
        ]
    assert expected == result

def test_prints_an_error_message_if_id_is_negative():
    script = [
        "insert -1 cstack foo@bar.com",
        "select",
        ".exit",
    ]
    result = run_script(script)
    expected = [
        "db > ID must be positive.",
        "db > Executed.",
        "db > "
    ]

def test_keeps_data_after_closing_connection():
    result1 = run_script([
        "insert 1 user1 person1@example.com",
        ".exit",
    ])    
    expected = [
    "db > Executed.",
    "db > ",
    ]

    assert expected == result1

    result2 = run_script([
        "select",
        ".exit",
    ])

    expected2 = [
        "db > (1, user1, person1@example.com)",
        "Executed.",
        "db > " 
    ]

    assert expected2 == result2
