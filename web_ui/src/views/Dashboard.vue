<template>
  <div>
    <el-row :gutter="16" style="margin-bottom:16px">
      <el-col :span="6">
        <el-card shadow="hover">
          <div class="stat-label">{{ $t('dashboard.proxyStatus') }}</div>
          <div class="stat-value">
            <el-tag :type="stats.proxy_running ? 'success' : 'danger'">
              {{ stats.proxy_running ? $t('dashboard.running') : $t('dashboard.stopped') }}
            </el-tag>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover">
          <div class="stat-label">{{ $t('dashboard.v2raynStatus') }}</div>
          <div class="stat-value">
            <el-tag :type="stats.v2rayn_status === 'running' ? 'success' : 'danger'">
              {{ stats.v2rayn_status === 'running' ? $t('dashboard.running') : $t('dashboard.stopped') }}
            </el-tag>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover">
          <div class="stat-label">{{ $t('dashboard.activeConns') }}</div>
          <div class="stat-value">{{ stats.active_connections || 0 }}</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover">
          <div class="stat-label">{{ $t('dashboard.totalConns') }}</div>
          <div class="stat-value">{{ stats.total_connections || 0 }}</div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" style="margin-bottom:16px">
      <el-col :span="12">
        <el-card>
          <template #header>{{ $t('dashboard.directTraffic') }}</template>
          <div>UP: {{ formatBytes(stats.direct_bytes_up) }} | DOWN: {{ formatBytes(stats.direct_bytes_down) }}</div>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card>
          <template #header>{{ $t('dashboard.proxyTraffic') }}</template>
          <div>UP: {{ formatBytes(stats.proxy_bytes_up) }} | DOWN: {{ formatBytes(stats.proxy_bytes_down) }}</div>
        </el-card>
      </el-col>
    </el-row>

    <el-card>
      <template #header>{{ $t('dashboard.recentLogs') }}</template>
      <el-table :data="recentLogs" size="small" max-height="300">
        <el-table-column prop="ts" label="Time" width="180">
          <template #default="{ row }">{{ formatTime(row.ts) }}</template>
        </el-table-column>
        <el-table-column prop="level" label="Level" width="80">
          <template #default="{ row }">
            <el-tag :type="row.level === 'error' ? 'danger' : row.level === 'warn' ? 'warning' : 'info'" size="small">
              {{ row.level }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="msg" label="Message" />
      </el-table>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import request from '../utils/request'

const stats = ref<any>({})
const recentLogs = ref<any[]>([])
let timer: any

function formatBytes(b: number) {
  if (!b) return '0 B'
  if (b < 1024) return b + ' B'
  if (b < 1048576) return (b / 1024).toFixed(1) + ' KB'
  return (b / 1048576).toFixed(1) + ' MB'
}

function formatTime(ts: number) {
  return new Date(ts).toLocaleString()
}

async function fetchStats() {
  try {
    const { data } = await request.get('/api/dashboard/stats')
    stats.value = data
  } catch {}
}

async function fetchLogs() {
  try {
    const { data } = await request.get('/api/dashboard/recent_logs?count=20')
    recentLogs.value = Array.isArray(data) ? data : []
  } catch {}
}

onMounted(() => {
  fetchStats()
  fetchLogs()
  timer = setInterval(() => { fetchStats(); fetchLogs() }, 5000)
})

onUnmounted(() => clearInterval(timer))
</script>

<style scoped>
.stat-label { color: #909399; font-size: 14px; margin-bottom: 8px }
.stat-value { font-size: 24px; font-weight: bold }
</style>
