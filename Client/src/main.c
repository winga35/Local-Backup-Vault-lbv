#include <stdio.h>

#include "defines/lbv_defines.h"
#include "ui_client.h"

int main(void)
{
	int rc;

	rc = ui_client_main(LBV_SOCK_PATH);

	if (rc == UI_CLIENT_NO_DAEMON)
	{
		fprintf(stderr, "lbv-client: no hay ningun daemon escuchando en %s\n",
			LBV_SOCK_PATH);
		fprintf(stderr, "            arrancalo con: lbv-daemon &\n");
		return LBV_EXIT_ERROR;
	}

	if (rc == UI_CLIENT_BUSY)
	{
		fprintf(stderr, "lbv-client: ya hay una UI conectada a este daemon\n");
		return LBV_EXIT_ERROR;
	}

	if (rc == UI_CLIENT_ERR_UI)
	{
		fprintf(stderr, "lbv-client: no se pudo inicializar la terminal\n");
		return LBV_EXIT_ERROR;
	}

	if (rc != UI_CLIENT_OK)
	{
		fprintf(stderr, "lbv-client: fallo la conexion (%d)\n", rc);
		return LBV_EXIT_ERROR;
	}

	return LBV_EXIT_OK;
}
