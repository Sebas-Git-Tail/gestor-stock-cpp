package com.sebastian.comun;

import androidx.core.content.FileProvider;

// Una clase propia para el FileProvider: así no choca con el de Qt (androidx.core.content.FileProvider,
// «qtprovider»), porque Android no deja declarar dos proveedores con la misma clase.
public class ProveedorArchivos extends FileProvider
{
}
