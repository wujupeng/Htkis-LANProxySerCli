<template>
  <el-container style="height: 100vh">
    <el-aside width="220px" style="background: #001529">
      <div class="logo">{{ $t('app.title') }}</div>
      <el-menu :default-active="$route.path" router dark background-color="#001529"
               text-color="#fff" active-text-color="#409eff">
        <el-menu-item index="/dashboard">
          <el-icon><Monitor /></el-icon>{{ $t('nav.dashboard') }}
        </el-menu-item>
        <el-menu-item index="/users">
          <el-icon><User /></el-icon>{{ $t('nav.users') }}
        </el-menu-item>
        <el-menu-item index="/rules">
          <el-icon><Guide /></el-icon>{{ $t('nav.rules') }}
        </el-menu-item>
        <el-menu-item index="/v2rayn">
          <el-icon><Connection /></el-icon>{{ $t('nav.v2rayn') }}
        </el-menu-item>
        <el-menu-item index="/settings">
          <el-icon><Setting /></el-icon>{{ $t('nav.settings') }}
        </el-menu-item>
        <el-menu-item index="/logs">
          <el-icon><Document /></el-icon>{{ $t('nav.logs') }}
        </el-menu-item>
      </el-menu>
    </el-aside>
    <el-container>
      <el-header style="display:flex;align-items:center;justify-content:space-between;background:#fff;border-bottom:1px solid #eee">
        <span style="font-size:18px;font-weight:bold">{{ $t('nav.' + String($route.name)) }}</span>
        <div>
          <el-button text @click="toggleLocale">{{ locale === 'zh-CN' ? 'EN' : '中文' }}</el-button>
          <el-button text type="danger" @click="logout">Logout</el-button>
        </div>
      </el-header>
      <el-main><router-view /></el-main>
    </el-container>
  </el-container>
</template>

<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { Monitor, User, Guide, Connection, Setting, Document } from '@element-plus/icons-vue'
import router from '../router'

const { locale } = useI18n()

function toggleLocale() {
  locale.value = locale.value === 'zh-CN' ? 'en' : 'zh-CN'
  localStorage.setItem('locale', locale.value)
}

function logout() {
  localStorage.removeItem('token')
  router.push('/login')
}
</script>

<style scoped>
.logo { color: #fff; font-size: 16px; font-weight: bold; padding: 20px 16px; text-align: center; }
</style>
