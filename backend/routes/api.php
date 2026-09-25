<?php

use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;
use App\Http\Controllers\Api\ClientDeviceController;

Route::get('/user', function (Request $request) {
    return $request->user();
})->middleware('auth:sanctum');

Route::post('/client/register', [ClientDeviceController::class, 'registerClient']);
Route::post('/client/update', [ClientDeviceController::class, 'updateStatus']);
Route::get('/clients', [ClientDeviceController::class, 'getClients']);
Route::post('/updatestatus', [ClientDeviceController::class, 'updateOnlineStatus']);
