<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;

class ClientDevice extends Model
{
    use HasFactory;

    protected $fillable = [
        'device_token',
        'computer_name',
        'nickname',
        'os_platform',
        'architecture',
        'public_ip',
        'status',
        'last_seen_at',
    ];

    protected $casts = [
        'last_seen_at' => 'datetime',
    ];
}
