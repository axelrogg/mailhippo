#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libetpan/libetpan.h>

static void check_error(int result, char *msg) {
    switch (result) {
        // IMAP
        case MAILIMAP_NO_ERROR:
            return;
        case MAILIMAP_NO_ERROR_AUTHENTICATED:
            return;
        case MAILIMAP_NO_ERROR_NON_AUTHENTICATED:
            return;
        default:
            fprintf(stderr, "Unknown error: %s\n", msg);
            exit(EXIT_FAILURE);
    }
}

static void display_subject(struct mailimf_subject *subject) {
    printf("%s\n", subject->sbj_value);
}

static void parse_encoded_subject(struct mailimf_subject *subject) {
    if (subject->sbj_value[0] != '=') {
        printf("'%s'", subject->sbj_value);
        return;
    }

    size_t subject_size    = strlen(subject->sbj_value);
    int    qmcount = 0;  // Question mark count

    int charset_len  = 0;
    int encoding_len = 0;
    int encoded_len  = 0;

    for (int i = 1; i < subject_size - 1; i++) {
        if (subject->sbj_value[i] == '?') {
            qmcount++;
            continue;
        } 
        switch (qmcount) {
        case 1:
            charset_len++;
            break;
        case 2:
            encoding_len++;
            break;
        case 3:
            encoded_len++;
            break;
        default:
            break;
        }
    }

    char charset[charset_len];
    char encoding[encoding_len];
    char encoded[encoded_len];

    int charset_visit = 0;
    int encoding_visit = 0;
    int encoded_visit = 0;

    qmcount = 0;
    for (int i = 1; i < subject_size - 1; i++) {
        if (subject->sbj_value[i] == '?') {
            qmcount++;
            continue;
        } 
        switch (qmcount) {
            case 1:
                charset[charset_visit++] = subject->sbj_value[i];
                break;
            case 2:
                encoding[encoding_visit++] = subject->sbj_value[i];
                break;
            case 3:
                encoded[encoded_visit++] = subject->sbj_value[i];
                break;
            default:
                break;
        }
    }
    printf("%s", encoded);
}

static void display_field(struct mailimf_field *field) {
    switch (field->fld_type) {
        case MAILIMF_FIELD_ORIG_DATE:
            // printf("Date: ");
            // display_orig_date(field->fld_data.fld_orig_date);
            // printf("\n");
            break;
        case MAILIMF_FIELD_FROM:
            // printf("From: ");
            // display_from(field->fld_data.fld_from);
            // printf("\n");
            break;
        case MAILIMF_FIELD_TO:
            // printf("To: ");
            // display_to(field->fld_data.fld_to);
            // printf("\n");
            break;
        case MAILIMF_FIELD_CC:
            // printf("Cc: ");
            // display_cc(field->fld_data.fld_cc);
            // printf("\n");
            break;
        case MAILIMF_FIELD_SUBJECT:
            printf("Subject: ");
            parse_encoded_subject(field->fld_data.fld_subject);
            printf("\n");
            break;
        case MAILIMF_FIELD_MESSAGE_ID:
            // printf("Message-ID: %s\n", field->fld_data.fld_message_id->mid_value);
            break;
    }
}

static void display_fields(struct mailimf_fields *fields) {
    clistiter *cur;

    cur = clist_begin(fields->fld_list);
    while (cur != NULL) {
        struct mailimf_field *field;
        field = clist_content(cur);
        display_field(field);
        cur = clist_next(cur);
    }
}

static void display_mime(struct mailmime *mime) {
    clistiter *cur;

    switch (mime->mm_type) {
        case MAILMIME_SINGLE:
            printf("single part\n");
            break;
        case MAILMIME_MULTIPLE:
            printf("multipart\n");
            break;
        case MAILMIME_MESSAGE:
            printf("message\n");
            break;
    }

    if (mime->mm_mime_fields != NULL) {
        if (clist_begin(mime->mm_mime_fields->fld_list) != NULL) {
            printf("MIME headers begin\n");
            // display_mime_fields(mime->mm_mime_fields);
            printf("MIME headers end\n");
        }
    }

    // display_mime_content(mime->mm_content_type);

    switch (mime->mm_type) {
        case MAILMIME_SINGLE:
            // display_mime_data(mime->mm_data.mm_single);
            break;

        case MAILMIME_MULTIPLE:
            // for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL; cur = clist_next(cur)) {
            //     display_mime(clist_content(cur));
            // }
            break;

        case MAILMIME_MESSAGE:
            if (mime->mm_data.mm_message.mm_fields) {
                if (clist_begin(mime->mm_data.mm_message.mm_fields->fld_list) != NULL) {
                    printf("----headers begin----\n");
                    display_fields(mime->mm_data.mm_message.mm_fields);
                    printf("----headers end----\n");
                }

                //if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
                //    display_mime(mime->mm_data.mm_message.mm_msg_mime);
                //}
                break;
            }
    }
}


static char *get_msg_att_msg_content(struct mailimap_msg_att *msg_att, size_t *p_msg_size) {
    clistiter *cur;

    /*iterate on each result of one given message */
    for(cur = clist_begin(msg_att->att_list); cur != NULL; cur = clist_next(cur)) {
        struct mailimap_msg_att_item *item;

        item = clist_content(cur);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
            continue;
        }

        if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_BODY_SECTION) {
            continue;
        }

        *p_msg_size = item->att_data.att_static->att_data.att_body_section->sec_length;
        return item->att_data.att_static->att_data.att_body_section->sec_body_part;
    }
    return NULL;
}

static char *get_message_content(clist *fetch_result, size_t *p_msg_size) {
    clistiter *cur;

    // for each message (there will probably be only one message)
    cur = clist_begin(fetch_result);
    while (cur != NULL) {
        struct mailimap_msg_att *msg_att;
        size_t msg_size;
        char *msg_content;
        int mime_result;

        msg_att = clist_content(cur);
        msg_content = get_msg_att_msg_content(msg_att, &msg_size);
        if (msg_content == NULL) {
            cur = clist_next(cur);
            continue;
        }
        *p_msg_size = msg_size;

        // Get MIME info from message content
        struct mailmime *mime;
        size_t current_mime_index = 0;

        mime_result = mailmime_parse(msg_content, msg_size, &current_mime_index, &mime);
        check_error(mime_result, "could not parse MIME info");

        display_mime(mime);
        cur = clist_next(cur);
        return msg_content;
    }
    return NULL;
}

static void fetch_msg(struct mailimap *imap, uint32_t uid) {
    struct mailimap_set *set;
    struct mailimap_section *section;
    // char filename[512];
    size_t msg_len;
    char *msg_content;
    char *msg_subject;
    struct mailimap_fetch_type *fetch_type;
    struct mailimap_fetch_att *fetch_att;
    int result;
    clist *fetch_result;
    struct stat stat_info;
    struct mailimap_msg_att *att;

    // result = stat(filename, &stat_info);
    // if (result == 0) {
    //     // already cached
    //     printf("%u is already fetched\n", (unsigned int) uid);
    //     return;
    // }

    set = mailimap_set_new_single(uid);
    fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    section = mailimap_section_new(NULL);
    fetch_att = mailimap_fetch_att_new_body_peek_section(section);
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);

    result = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);

    check_error(result, "could not fetch");
    printf("fetch %u\n", (unsigned int) uid);

    msg_content = get_message_content(fetch_result, &msg_len);
    if (msg_content == NULL) {
        fprintf(stderr, "no content\n");
        mailimap_fetch_list_free(fetch_result);
        return;
    }

    // printf("FOUND: %s", msg_content);
    // printf("%u has been fetched\n", (unsigned int) uid);

    mailimap_fetch_list_free(fetch_result);
}

static uint32_t get_uid(struct mailimap_msg_att *msg_att) {
    clistiter *cur;

    // Iterate on each result of one given message
    for (cur = clist_begin(msg_att->att_list); cur != NULL; cur = clist_next(cur)) {
        struct mailimap_msg_att_item *item;
        item = clist_content(cur);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
            continue;
        }
        if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID) {
            continue;
        }
        return item->att_data.att_static->att_data.att_uid;
    }
    return 0;
}

/**
 * @brief Fetch and processes all messages in a given mailbox using the IMAP
 *        protocol.
 * @param imap
 * 
 * @note
 *      - If we want to fetch messages within a specific range we must change
 *        the set interval `last` value to something other than 0.
*/
static void fetch_messages(struct mailimap *imap) {
    struct mailimap_set        *set;
    struct mailimap_fetch_type *fetch_type;
    struct mailimap_fetch_att  *fetch_att;
    clist                      *fetch_result;
    clistiter                  *cur;
    int                         result;

    // As improvement UIDVALIDITY should be read and the message cache should be cleaned
    // if the UIDVALIDITY is not the same
    set = mailimap_set_new_interval(1, 0);  // Select messages within range of 1:*
    fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    fetch_att = mailimap_fetch_att_new_uid();
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);  // Append the `fetch_att` to the `fetch_type->ft_data.ft_fetch_att_list`

    result = mailimap_fetch(imap, set, fetch_type, &fetch_result);
    check_error(result, "could not fetch");

    cur = clist_begin(fetch_result);
    int i = 0;
    while (cur != NULL) {
        if (i > 1) {
            break;
        }
        struct mailimap_msg_att *msg_att;
        uint32_t uid;

        msg_att = clist_content(cur);
        uid = get_uid(msg_att);
        if (uid == 0)
            continue;
        fetch_msg(imap, uid);

        cur = clist_next(cur);
        i++;
    }
    mailimap_fetch_list_free(fetch_result);
}

int main(void) {
    struct mailimap *imap;
    int result;

    char *imap_ssl_host = "imap.gmail.com";
    uint16_t imap_ssl_port = 993;

    imap = mailimap_new(0, NULL);
    result = mailimap_ssl_connect(imap, imap_ssl_host, imap_ssl_port);
    fprintf(stderr, "connect: %i\n", result);
    check_error(result, "could not connect to server");

    result = mailimap_login(imap, username, password);
    check_error(result, "could not login");

    result = mailimap_select(imap, "INBOX");
    check_error(result, "could not select INBOX");

    fetch_messages(imap);

    // Get out and clean up
    mailimap_logout(imap);
    mailimap_free(imap);

    return 0;
}
