/** rdda_ecat.c */

#include "rdda_ecat.h"

/* SOEM global vars */
char IOmap[4096];
//boolean inOP;
//boolean needlf;
//uint8 currentgroup = 0;

/** Locate and identify EtherCAT slaves.
 *
 * @param rdda_slave    = Slave index group.
 * @return 0 on success.
 */
static int slaveIdentify(ecat_slave *slave) {
    uint16 idx = 0;
    int buf = 0;
    for (idx = 1; idx <= ec_slavecount; idx++) {
        /* BEL motor drive */
        if ((ec_slave[idx].eep_man == 0x000000ab) && (ec_slave[idx].eep_id == 0x00001110)) {
            uint32 serial_num;
            buf = sizeof(serial_num);
            ec_SDOread(idx, 0x1018, 4, FALSE, &buf, &serial_num, EC_TIMEOUTRXM);

            /* motor1 */
            if (serial_num == 0x2098302) {
                //network->motor[0] = idx;
                slave->bel[0].slave_id = idx;
                /* CompleteAccess disabled for BEL drive */
                //ec_slave[slaveIdx].CoEdetails ^= ECT_COEDET_SDOCA;
                /* Set PDO mapping */
                printf("Found %s at position %d\n", ec_slave[idx].name, idx);
                if (1 == mapMotorPDOs_callback(idx)) {
                    fprintf(stderr, "Motor1 mapping failed!\n");
                    exit(1);
                }
            }
            /* motor2 */
            if (serial_num == 0x2098303) {
                slave->bel[1].slave_id = idx;
                /* CompleteAccess disabled for BEL drive */
                //ec_slave[slaveIdx].CoEdetails ^= ECT_COEDET_SDOCA;
                /* Set PDO mapping */
                printf("Found %s at position %d\n", ec_slave[idx].name, idx);
                if (1 == mapMotorPDOs_callback(idx)) {
                    fprintf(stderr, "Motor2 mapping failed!\n");
                    exit(1);
                }
            }
        }
        /* pressure sensor */
        if ((ec_slave[idx].eep_man == 0x00000002) && (ec_slave[idx].eep_id == 0x0c1e3052)) {
            slave->el3102.slave_id = idx;
        }
    }

    return 0;
}

/** Set up EtherCAT NIC and state machine to request all slaves to work properly.
 *
 * @param ifnameptr = NIC interface pointer
 * @return 0.
 */
ecat_slave *initEcatConfig(void *ifnameptr) {
    char *ifname  = (char *)ifnameptr;

//    inOP = FALSE;
//    needlf = FALSE;
    int expectedWKC;
    int wkc;

    /* Initialize data structure */
    printf("Init data structure.\n");
    ecat_slave *ecatSlave;
    ecatSlave = (ecat_slave *)malloc(sizeof(ecat_slave));
    if (ecatSlave == NULL) {
        return NULL;
    }

    printf("Begin network configuration\n");
    /* Initialize SOEM, bind socket to ifname */
    if (ec_init(ifname)) {
	    printf("Socket connection on %s succeeded.\n", ifname);
    }
    else {
	    fprintf(stderr, "No socket connection on %s.\nExcecuted as root\n", ifname);
	    exit(1);
    }

    /* Find and configure slaves */
    if (ec_config_init(FALSE) > 0) {
	    printf("%d slaves found and configured.\n", ec_slavecount);
    }
    else {
	    fprintf(stderr, "No slaves found!\n");
	    ec_close();
	    exit(1);
    }

    /* Request for PRE-OP mode */
    ec_statecheck(0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE);

    /* Locate slaves */
    slaveIdentify(ecatSlave);
    printf("psensor_id: %d\n", ecatSlave->el3102.slave_id);
    if (ecatSlave->bel[0].slave_id == 0 || ecatSlave->bel[1].slave_id == 0 || ecatSlave->el3102.slave_id == 0) {
        fprintf(stderr, "Slaves identification failure!");
        exit(1);
    }

    /* If Complete Access (CA) disabled => auto-mapping work */
    ec_config_map(&IOmap);

    /* Let DC off for the time being */
    ec_configdc(); // DC should be launched for each identified slave

    printf("Slaves mapped, state to SAFE_OP\n");
    /* Wait for all salves to reach SAFE_OP state */
    ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

    initMotor(ecatSlave->bel[0].slave_id);
    initMotor(ecatSlave->bel[1].slave_id);
    printf("Slaves initialized, state to OP\n");

    /* Check if all slaves are working properly */
    expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
    printf("Calculated workcounter %d\n", expectedWKC);

    /* Request for OP mode */
    ec_slave[0].state = EC_STATE_OPERATIONAL;
    /* Send one valid process data to make outputs in slave happy */
    ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);
    if (wkc < expectedWKC) {
        fprintf(stderr, "WKC failure.\n");
        exit(1);
    }

    /* Request OP state for all slaves */
    ec_writestate(0);
    /* Wait for all slaves to reach OP state */
    ec_statecheck(0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);

    for (int mot_id = 0; mot_id < 2; mot_id ++) {
        ecatSlave->bel[mot_id].in_motor = (motor_input *)ec_slave[ecatSlave->bel[mot_id].slave_id].inputs;
    }
    ecatSlave->el3102.in_analog = (analog_input *)ec_slave[ecatSlave->el3102.slave_id].inputs;

    /* Recheck */
    if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
        printf("Operational state reached for all slaves\n");
        return ecatSlave;
    }
    else {
        ec_close();
        fprintf(stderr, "Operational state failed\n");
        exit(1);
    }
}

//#define EC_TIMEOUTMON 500

/** Error handling in OP mode.
 *
 * @param ptr   =   NULL;
 */
/*
void ecatcheck(void *ptr)
{
    int slave;
    (void)ptr;

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
                needlf = FALSE;
                printf("\n");
            }
            // one ore more slaves are not responding
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
                if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
                {
                    ec_group[currentgroup].docheckstate = TRUE;
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                    {
                        printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    }
                    else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                    {
                        printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    }
                    else if(ec_slave[slave].state > EC_STATE_NONE)
                    {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                        {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d reconfigured\n",slave);
                        }
                    }
                    else if(!ec_slave[slave].islost)
                    {
                        // re-check state
                        ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (ec_slave[slave].state == EC_STATE_NONE)
                        {
                            ec_slave[slave].islost = TRUE;
                            printf("ERROR : slave %d lost\n",slave);
                        }
                    }
                }
                if (ec_slave[slave].islost)
                {
                    if(ec_slave[slave].state == EC_STATE_NONE)
                    {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON))
                        {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d recovered\n",slave);
                        }
                    }
                    else
                    {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d found\n",slave);
                    }
                }
            }
            if(!ec_group[currentgroup].docheckstate)
                printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}
*/