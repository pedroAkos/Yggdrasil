#!/usr/bin/env bash

ip[0]=169.254.242.48
ip[1]=169.254.62.239
ip[2]=169.254.242.146
ip[3]=169.254.197.148
ip[4]=169.254.231.54
ip[5]=169.254.144.17
ip[6]=169.254.194.207
ip[7]=169.254.156.201
ip[8]=169.254.103.239
ip[9]=169.254.26.117
ip[10]=169.254.1.137
ip[11]=169.254.119.185
ip[12]=169.254.155.175
ip[13]=169.254.17.33
ip[14]=169.254.154.203
ip[15]=169.254.80.5
ip[16]=169.254.47.35
ip[17]=169.254.54.153
ip[18]=169.254.206.169
ip[19]=169.254.48.133
ip[20]=169.254.232.253

declare -A tree
tree=( [${ip[0]}]="" )

SERV_COMMAND="hostname"
COMMAND="hostname"

script="ssh 192.168.1.101 \
     '$COMMAND; \
     ssh ${ip[1]} '$COMMAND'; \
     ssh ${ip[2]} '$COMMAND'; \
     ssh ${ip[3]} '$COMMAND'; \
     ssh ${ip[4]} '$COMMAND'; \
     ssh ${ip[5]} '$COMMAND'; \
     ssh ${ip[6]} '$COMMAND'; \
     ssh ${ip[7]} '$COMMAND'; \
     ssh ${ip[9]} '$COMMAND'; \
     ssh ${ip[10]} '$COMMAND'; \
     ssh ${ip[11]} '$COMMAND'; \
     ssh ${ip[12]} '$COMMAND'; \
     ssh ${ip[13]} '$COMMAND'; \
     ssh ${ip[14]} '$COMMAND'; \
     ssh ${ip[15]} '$COMMAND'; \
     ssh ${ip[16]} '$COMMAND'; \
     ssh ${ip[17]} '$COMMAND'; \
     ssh ${ip[18]} '$COMMAND'; \
     ssh ${ip[19]} '$COMMAND'; \
     ssh ${ip[20]} '$COMMAND' '"

#echo $script



SERV_COMMAND="sudo ./execute_program Ts"
COMMAND="sudo ./execute_program TsBenchA"

SERV_COMMAND="sudo ./execute_program YggTs"
COMMAND="sudo ./execute_program YggTsBenchB"

SERV_COMMAND="sudo ./execute_program YggTs 0"
COMMAND="sudo ./execute_program YggTsBenchA 0"


#SERV_COMMAND="hostname"
#COMMAND="touch control.file"

#SERV_COMMAND='echo "hello"'
#COMMAND='echo "hello"'

script="ssh 192.168.1.101 \
'$SERV_COMMAND; \
     ssh ${ip[19]} '$COMMAND \
     '; \
     ssh ${ip[1]} '$COMMAND; \
            ssh ${ip[15]} '$COMMAND; \
                 ssh ${ip[3]} '$COMMAND; \
                    ssh ${ip[6]} '$COMMAND; \
                        ssh ${ip[14]} '$COMMAND; \
                            ssh ${ip[13]} '$COMMAND; \
                                ssh ${ip[16]} '$COMMAND \
                                '; \
                                ssh ${ip[12]} '$COMMAND \
                                ' \
                             ' \
                        ' \
                    ' \
                 ' \
            '; \
            ssh ${ip[2]} '$COMMAND \
            ' \
     '; \
     ssh ${ip[5]} '$COMMAND; \
         ssh ${ip[18]} '$COMMAND; \
                 ssh ${ip[11]} '$COMMAND; \
                    ssh ${ip[10]} '$COMMAND \
                    ' \
                 '; \
                 ssh ${ip[9]} '$COMMAND; \
                    ssh ${ip[8]} '$COMMAND; \
                        ssh ${ip[20]} '$COMMAND \
                        ' \
                    '; \
                    ssh ${ip[7]} '$COMMAND \
                    ' \
                 '; \
                 ssh ${ip[4]} '$COMMAND \
                 ' \
         '; \
         ssh ${ip[17]} '$COMMAND' \
     '\
'"

script="ssh 192.168.1.101 \
'$SERV_COMMAND; \
     ssh ${ip[19]} '$COMMAND \
     '; \
     ssh ${ip[1]} '$COMMAND; \
            ssh ${ip[15]} '$COMMAND; \
                 ssh ${ip[3]} '$COMMAND; \
                    ssh ${ip[6]} '$COMMAND; \
                        ssh ${ip[14]} '$COMMAND; \
                            ssh ${ip[13]} '$COMMAND; \
                                ssh ${ip[16]} '$COMMAND \
                                '; \
                                ssh ${ip[12]} '$COMMAND \
                                ' \
                             ' \
                        ' \
                    ' \
                 ' \
            '; \
            ssh ${ip[2]} '$COMMAND \
            ' \
     '; \
     ssh ${ip[5]} '$COMMAND; \
         ssh ${ip[18]} '$COMMAND; \
                 ssh ${ip[11]} '$COMMAND; \
                    ssh ${ip[10]} '$COMMAND \
                    ' \
                 '; \
                 ssh ${ip[9]} '$COMMAND; \
                    ssh ${ip[8]} '$COMMAND; \
                        ssh ${ip[20]} '$COMMAND \
                        ' \
                    '; \
                    ssh ${ip[7]} '$COMMAND \
                    ' \
                 '; \
                 ssh ${ip[4]} '$COMMAND \
                 ' \
         '; \
         ssh ${ip[17]} '$COMMAND' \
     '\
'"


echo $script

D_COMMAND="hostname"

script="ssh 192.168.1.101 \
' \
     ssh ${ip[19]} '$COMMAND \
     '; \
     ssh ${ip[1]} ' \
            $D_COMMAND; \
            ssh ${ip[15]} ' \
                $D_COMMAND; \
                 ssh ${ip[3]} ' \
                    $D_COMMAND; \
                    ssh ${ip[6]} ' \
                        $D_COMMAND; \
                        ssh ${ip[14]} ' \
                            $D_COMMAND; \
                            ssh ${ip[13]} ' \
                                $D_COMMAND; \
                                ssh ${ip[16]} '$D_COMMAND;$COMMAND \
                                '; \
                                ssh ${ip[12]} '$D_COMMAND;$COMMAND \
                                '; \
                                $D_COMMAND; \
                                $COMMAND \
                             '; \
                             $D_COMMAND; \
                            $COMMAND \
                        '; \
                        $D_COMMAND; \
                        $COMMAND \
                    '; \
                    $D_COMMAND; \
                    $COMMAND \
                 '; \
                 $D_COMMAND; \
                 $COMMAND \
            '; \
            ssh ${ip[2]} '$D_COMMAND; $COMMAND \
            '; \
            $D_COMMAND; \
            $COMMAND \
     '; \
     ssh ${ip[5]} ' \
         ssh ${ip[18]} ' \
                 ssh ${ip[11]} ' \
                    ssh ${ip[10]} '$COMMAND \
                    '; \
                    $COMMAND \
                 '; \
                 ssh ${ip[9]} ' \
                    ssh ${ip[8]} ' \
                        ssh ${ip[20]} '$COMMAND \
                        '; \
                        $COMMAND \
                    '; \
                    ssh ${ip[7]} '$COMMAND \
                    '; \
                    $COMMAND \
                 '; \
                 ssh ${ip[4]} '$COMMAND \
                 '; \
                 $COMMAND \
         '; \
         ssh ${ip[17]} '$COMMAND \
         '; \
         $COMMAND \
     '; \
$SERV_COMMAND'"

ssh 192.168.1.101 'sudo ./execute_program YggTs 0; ssh 169.254.48.133 'sudo ./execute_program YggTsBenchA 0 '; ssh 169.254.62.239 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.80.5 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.197.148 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.194.207 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.154.203 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.17.33 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.47.35 'sudo ./execute_program YggTsBenchA 0 '; ssh 169.254.155.175 'sudo ./execute_program YggTsBenchA 0 ' ' ' ' ' '; ssh 169.254.242.146 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.144.17 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.206.169 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.119.185 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.1.137 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.26.117 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.103.239 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.232.253 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.156.201 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.231.54 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.54.153 'sudo ./execute_program YggTsBenchA 0' ''
ssh 192.168.1.101 'sudo ./executecp _program YggTs 0; ssh 169.254.48.133 'sudo ./execute_program YggTsBenchA 0 '; ssh 169.254.62.239 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.80.5 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.197.148 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.194.207 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.154.203 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.17.33 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.47.35 'sudo ./execute_program YggTsBenchA 0 '; ssh 169.254.155.175 'sudo ./execute_program YggTsBenchA 0 ' ' ' ' ' '; ssh 169.254.242.146 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.144.17 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.206.169 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.119.185 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.1.137 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.26.117 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.103.239 'sudo ./execute_program YggTsBenchA 0; ssh 169.254.232.253 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.156.201 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.231.54 'sudo ./execute_program YggTsBenchA 0 ' '; ssh 169.254.54.153 'sudo ./execute_program YggTsBenchA 0' ''


ssh -t 169.254.62.239 ' ssh -t 169.254.242.146  "ssh -t 169.254.197.148 \"ssh -t 169.254.231.54 \" ssh -t 169.254.144.17 \" ssh -t 169.254.194.207 \"hostname\"  ;hostname\" ;hostname\";  hostname\" ; hostname\" ; hostname '