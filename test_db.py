import subprocess
import os

def run_script(commands, clear_db = True):
    if os.path.exists('test.db') and clear_db:
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
    expected = [
            "db > Need to implement updating parent after split",
            ""
        ]
    assert expected == result[-2:]

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
    ], False)

    expected2 = [
        "db > (1, user1, person1@example.com)",
        "Executed.",
        "db > " 
    ]

    assert expected2 == result2

def test_prints_constants():
    result = run_script([
        ".constants",
        ".exit",
    ])

    expected = [
        "db > Constants:",
        "ROW_SIZE: 293",
        "COMMON_NODE_HEADER_SIZE: 6",
        "LEAF_NODE_HEADER_SIZE: 14",
        "LEAF_NODE_CELL_SIZE: 297",
        "LEAF_NODE_SPACE_FOR_CELLS: 4082",
        "LEAF_NODE_MAX_CELLS: 13",
        "db > "
        ]
    
    assert expected == result


def test_allows_printing_out_the_structure_of_a_one_node_btree():
    script = [f"insert {i} user{i} person{i}@example.com" for i in [3,1,2]]
    script.extend([".btree", ".exit"])
    result = run_script(script)
    expected = [
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Tree:",
        "- leaf (size 3)",
        "  - 1",
        "  - 2",
        "  - 3",
        "db > "
        ]
    
    assert expected == result


def test_prints_an_error_message_if_there_is_a_duplicate_id():
    script = [
      "insert 1 user1 person1@example.com",
      "insert 1 user1 person1@example.com",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expected = [
      "db > Executed.",
      "db > Error: Duplicate key.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > ",
    ]

    assert expected == result


def test_allows_printing_out_the_structure_of_a_3_leaf_node_btree():
    script = [f"insert {i} user{i} person{i}@example.com" for i in range(1,15)]
    script.append(".btree")
    script.append("insert 15 user15 person15@example.com")
    script.append(".exit")
    result = run_script(script)

    expected = [
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Tree:",
      "- internal (size 1)",
      "  - leaf (size 7)",
      "    - 1",
      "    - 2",
      "    - 3",
      "    - 4",
      "    - 5",
      "    - 6",
      "    - 7",
      "  - key 7",
      "  - leaf (size 7)",
      "    - 8",
      "    - 9",
      "    - 10",
      "    - 11",
      "    - 12",
      "    - 13",
      "    - 14",
      "db > Executed.",
      "db > "
    ]

    assert result == expected


def test_prints_all_rows_in_a_multi_level_tree():
        
        script=[f"insert {i} user{i} person{i}@example.com" for i in range(1,16)]
        script.append("select")
        script.append(".exit")
    
        result = run_script(script, clear_db=True)
        
        expected = [
            "db > Executed.", "db > Executed.", "db > Executed.", 
            "db > Executed.", "db > Executed.", "db > Executed.", 
            "db > Executed.", "db > Executed.", "db > Executed.", 
            "db > Executed.", "db > Executed.", "db > Executed.", 
            "db > Executed.", "db > Executed.", "db > Executed.", 
            "db > (1, user1, person1@example.com)",
            "(2, user2, person2@example.com)",
            "(3, user3, person3@example.com)",
            "(4, user4, person4@example.com)",
            "(5, user5, person5@example.com)",
            "(6, user6, person6@example.com)",
            "(7, user7, person7@example.com)",
            "(8, user8, person8@example.com)",
            "(9, user9, person9@example.com)",
            "(10, user10, person10@example.com)",
            "(11, user11, person11@example.com)",
            "(12, user12, person12@example.com)",
            "(13, user13, person13@example.com)",
            "(14, user14, person14@example.com)",
            "(15, user15, person15@example.com)",
            "Executed.", "db > ",
        ]
    
        assert expected == result