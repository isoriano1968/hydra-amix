$0 !~ /^-/ {
	if ($2 != "-")
		printf "%s '#' expr\t\t\t{ cgen2( 0x%s, $3); }\n", $1, $2
	if ($3 != "-") {
		if ($4 == "-")
			printf "%s expr\t\t\t{ cgen3( 0x%s, $2); }\n", $1, $3
		else
			printf "%s expr\t\t\t{ cgenx( 0x%s, 0x%s, $2); }\n", $1, $4, $3
	}
	else if ($4 != "-")
		printf "%s expr		{ cgen2( 0x%s, $2); }\n", $1, $4
	if ($5 != "-")
		printf "%s A\t\t\t\t{ cgen1( 0x%s); }\n", $1, $5
	if ($6 != "-")
		printf "%s\t\t\t\t{ cgen1( 0x%s); }\n", $1, $6
	if ($7 != "-")
		printf "%s '[' expr ',' X ']'	{ cgen2( 0x%s, $3); }\n", $1, $7
	if ($8 != "-")
		printf "%s '[' expr ']' ',' Y	{ cgen2( 0x%s, $3); }\n", $1, $8
	if ($9 != "-")
		if ($10 != "-")
			printf "%s expr ',' X\t\t{ cgenx( 0x%s, 0x%s, $2); }\n", $1, $9, $10
		else
			printf "%s expr ',' X\t\t{ cgen2( 0x%s, $2); }\n", $1, $9
	else if ($10 != "-")
		printf "%s expr ',' X	{ cgen3( 0x%s, $2); }\n", $1, $10
	if ($11 != "-")
		if ($14 != "-")
			printf "%s expr ',' Y\t\t{ cgenx( 0x%s, 0x%s, $2); }\n", $1, $14, $11
		else
			printf "%s expr ',' Y\t\t{ cgen3( 0x%s, $2); }\n", $1, $11
	else if ($14 != "-")
		printf "%s expr ',' Y\t\t{ cgen2( 0x%s, $2); }\n", $1, $14
	if ($12 != "-")
		printf "%s expr\t\t\t{ cgen2r( 0x%s, $2); }\n", $1, $12
	if ($13 != "-")
		printf "%s '[' expr ']'		{ cgen3( 0x%s, $3); }\n", $1, $13
}
