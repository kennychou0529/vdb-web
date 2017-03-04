
// Returns true (1) if we successfully parsed the message
// or if we did not recognize it. Returns false (0) if something
// unexpected happened while parsing the message.
int vdb_handle_message(vdb_msg_t msg, vdb_status_t *status)
{
    vdb_status_t new_status = *status;
    // vdb_log("Got a message (%d): '%s'\n", msg.length, msg.payload);
    // This means the user pressed the 'continue' button
    if (msg.length == 1 && msg.payload[0] == 'c')
    {
        new_status.flag_continue = 1;
    }

    // Mouse click event
    if (msg.length > 1 && msg.payload[0] == 'm')
    {
        float x, y;
        vdb_assert(sscanf(msg.payload+1, "%f%f", &x, &y) == 2);
        new_status.mouse_click = 1;
        new_status.mouse_click_x = x;
        new_status.mouse_click_y = y;
    }

    // This is a status update that is sent at regular intervals
    if (msg.length > 1 && msg.payload[0] == 's')
    {
        char *str = msg.payload;
        int pos = 0 + 2;
        int got = 0;
        int i;
        int n;

        // read 'number of variables'
        vdb_assert(sscanf(str+pos, "%d%n", &n, &got) == 1);
        vdb_assert(n >= 0 && n < VDB_MAX_VAR_COUNT);

        // if there are no variables we are done!
        if (n == 0)
            return 1;

        pos += got+1; // read past int and space
        vdb_assert(pos < msg.length);

        for (i = 0; i < n; i++)
        {
            vdb_label_t label;
            float value;

            // read label
            vdb_assert(pos + VDB_LABEL_LENGTH < msg.length);
            vdb_copy_label(&label, str+pos);
            pos += VDB_LABEL_LENGTH;

            // read value
            vdb_assert(sscanf(str+pos, "%f%n", &value, &got) == 1);
            pos += got+1; // read past float and space

            // update variable @ ROBUSTNESS @ RACE CONDITION
            new_status.var_label[i] = label;
            new_status.var_value[i] = value;
        }
        new_status.var_count = n;
    }

    *status = new_status;
    return 1;
}
