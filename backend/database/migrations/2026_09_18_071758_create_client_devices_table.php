<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('client_devices', function (Blueprint $table) {
            $table->id();
            $table->uuid('device_token')->unique();
            $table->string('computer_name');
            $table->string('nickname')->unique();
            $table->string('os_platform')->nullable();
            $table->string('architecture')->nullable();
            $table->ipAddress('public_ip');
            $table->enum('status', ['online', 'offline', 'idle'])->default('online');
            $table->timestamp('last_seen_at');
            $table->timestamps();

            // Indexes for fast lookup performance
            $table->index('device_token');
            $table->index('status');
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('client_devices');
    }
};
