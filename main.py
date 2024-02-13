import sys
from typing import Any
from imaplib import IMAP4_SSL
from dataclasses import dataclass

@dataclass
class MailboxListResult:
    has_children: bool
    dir_delimeter: str
    hierarchy: str

def parse_mailbox_list_result(result: list[Any])-> list[MailboxListResult]:
    mailboxes: list[MailboxListResult] = []
    for list_item in result:
        assert isinstance(list_item, bytes)
        item = list_item.decode("utf-8")

        has_children_flag_start_i = 0
        has_children_flag_end_i = 0
        for i, val in enumerate(item):
            if val == "(":
                has_children_flag_start_i = i
                continue
            if val == ")":
                has_children_flag_end_i = i
                break

        has_children_flag = item[has_children_flag_start_i + 2:has_children_flag_end_i]
        has_children = "HasChildren" in has_children_flag

        rest = item[has_children_flag_end_i + 1:].lstrip().split(" ")
        dir_delimiter = rest[0][1]
        dir_hierarchy = bytes(rest[1].replace("&", "+"), "utf-8").decode("utf-7")
        assert isinstance(rest[1], str)
        mailboxes.append(MailboxListResult(has_children, dir_delimiter, dir_hierarchy))
    return mailboxes


def main():
    assert len(sys.argv) == 5
    args = [sys.argv[i] for i in range(5)]
    imap_ssl_host = args[1]
    imap_ssl_port = int(args[2])
    username = args[3]
    password = args[4]

    with IMAP4_SSL(imap_ssl_host, imap_ssl_port) as M:
        M.login(username, password)
        mailboxes_query_status, mailboxes_result = M.list()
        if mailboxes_query_status != "OK":
            raise Exception("not ok")

        mailboxes = parse_mailbox_list_result(mailboxes_result)
        for i in mailboxes:
            print(i)

if __name__ == "__main__":
    main()
